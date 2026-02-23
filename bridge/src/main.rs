mod config;
mod events;
mod grpc;
mod renode;
mod shm;

use std::sync::{Arc, Mutex};
use std::time::Duration;

use anyhow::Result;
use clap::Parser;
use tracing::{debug, error, info};

use config::Config;
use events::{GpioChangeEvent, ZmqPublisher};
use shm::gpio::{self, GpioPortState};
use shm::layout;
use shm::ShmRegion;

/// Ports to monitor (index, label).
const MONITORED_PORTS: &[(usize, &str)] = &[
    (layout::PORT_D, "D"), // LEDs on PD12-15
];

fn main() -> Result<()> {
    // Init logging
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "voltad=debug,info".into()),
        )
        .init();

    let cfg = Config::parse();
    info!("voltad starting");
    info!("  SHM path: {}", cfg.shm_path);
    info!("  ZMQ PUB:  {}", cfg.zmq_pub_addr);
    info!("  gRPC:     {}", cfg.grpc_addr);
    info!("  Renode:   {}", cfg.renode_addr);
    info!(
        "  Poll interval: {} μs",
        cfg.poll_interval_us
    );

    // Open SHM (retry until Renode initializes it)
    info!("Waiting for SHM to be initialized by Renode...");
    let shm = ShmRegion::open_with_retry(&cfg.shm_path, cfg.shm_size, 60)?;
    info!(
        "SHM initialized: {} ({} bytes)",
        cfg.shm_path, cfg.shm_size
    );

    // Bind ZMQ PUB
    let zmq_pub = ZmqPublisher::bind(&cfg.zmq_pub_addr)?;
    info!("ZMQ PUB bound: {}", cfg.zmq_pub_addr);

    // Connect to Renode telnet
    info!("Connecting to Renode telnet at {}...", cfg.renode_addr);
    let renode = renode::RenodeTelnet::connect_with_retry(&cfg.renode_addr, 30)?;
    info!("Renode connected via telnet");
    let renode = Arc::new(Mutex::new(renode));

    // Spawn gRPC server on Tokio runtime
    let grpc_addr = cfg.grpc_addr.clone();
    let renode_for_grpc = Arc::clone(&renode);
    std::thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .expect("Failed to build Tokio runtime for gRPC");
        rt.block_on(async move {
            let addr = grpc_addr
                .parse()
                .expect("Invalid gRPC address");
            let service =
                grpc::server::VoltaControlService::new(renode_for_grpc);
            info!("gRPC server listening on {}", grpc_addr);
            if let Err(e) = tonic::transport::Server::builder()
                .add_service(service.into_server())
                .serve(addr)
                .await
            {
                error!("gRPC server error: {e}");
            }
        });
    });

    // Run SHM monitor loop (blocking, main thread)
    info!("Starting SHM monitor loop ({}μs interval)", cfg.poll_interval_us);
    shm_monitor_loop(shm, zmq_pub, cfg.poll_interval_us)?;

    Ok(())
}

fn shm_monitor_loop(shm: ShmRegion, zmq_pub: ZmqPublisher, poll_us: u64) -> Result<()> {
    let interval = Duration::from_micros(poll_us);

    // Cached state per monitored port
    let mut cached: Vec<(usize, &str, GpioPortState)> = MONITORED_PORTS
        .iter()
        .map(|&(idx, label)| {
            (
                idx,
                label,
                GpioPortState {
                    output_state: 0,
                    input_state: 0,
                },
            )
        })
        .collect();

    let mut last_seq = shm.sequence();

    loop {
        std::thread::sleep(interval);

        let seq = shm.sequence();
        if seq == last_seq {
            continue;
        }
        last_seq = seq;

        // Check each monitored port for changes
        for entry in cached.iter_mut() {
            let (port_idx, port_label, ref mut prev) = *entry;
            let current = shm.gpio_port(port_idx);

            if current == *prev {
                continue;
            }

            let changed = prev.changed_pins(&current);

            // Publish an event for each changed pin
            for pin in 0..16u8 {
                if changed & (1 << pin) != 0 {
                    let state = current.pin_output(pin);
                    debug!(
                        "GPIO change: port={} pin={} state={}",
                        port_label,
                        pin,
                        if state { "HIGH" } else { "LOW" }
                    );

                    let event = GpioChangeEvent {
                        port: port_label.to_string(),
                        pin,
                        state,
                        sequence: seq,
                    };
                    if let Err(e) = zmq_pub.publish_gpio_change(&event) {
                        error!("ZMQ publish failed: {e}");
                    }
                }
            }

            *prev = current;
        }
    }
}
