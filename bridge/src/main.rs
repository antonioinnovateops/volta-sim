mod config;
mod events;
mod grpc;
mod renode;
mod shm;

use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use anyhow::Result;
use clap::Parser;
use tracing::{debug, error, info};

use config::Config;
use events::{GpioChangeEvent, UartDataEvent, ZmqPublisher};
use shm::gpio::GpioPortState;
use shm::layout;
use shm::ShmRegion;

/// GPIO ports to monitor (index, label).
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
    info!("  UART:     {}", cfg.uart_addr);
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

    // Spawn UART socket reader thread
    let (uart_tx, uart_rx) = mpsc::channel::<Vec<u8>>();
    let uart_addr = cfg.uart_addr.clone();
    std::thread::spawn(move || {
        info!("Connecting to UART terminal at {uart_addr}...");
        match renode::UartSocketReader::connect_with_retry(&uart_addr, 60) {
            Ok(mut reader) => {
                info!("UART terminal connected: {uart_addr}");
                loop {
                    match reader.read_available() {
                        Some(bytes) if !bytes.is_empty() => {
                            if uart_tx.send(bytes).is_err() {
                                break; // receiver dropped
                            }
                        }
                        Some(_) => {} // empty read (timeout), keep going
                        None => {
                            error!("UART terminal connection closed");
                            break;
                        }
                    }
                }
            }
            Err(e) => {
                error!("Failed to connect to UART terminal: {e}");
            }
        }
    });

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
    shm_monitor_loop(shm, zmq_pub, uart_rx, cfg.poll_interval_us)?;

    Ok(())
}

fn shm_monitor_loop(
    shm: ShmRegion,
    zmq_pub: ZmqPublisher,
    uart_rx: mpsc::Receiver<Vec<u8>>,
    poll_us: u64,
) -> Result<()> {
    let interval = Duration::from_micros(poll_us);

    // Cached GPIO state per monitored port
    let mut gpio_cached: Vec<(usize, &str, GpioPortState)> = MONITORED_PORTS
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

        // Check for UART data from socket terminal
        while let Ok(data) = uart_rx.try_recv() {
            let text = String::from_utf8_lossy(&data);
            debug!(
                "UART[2] data: {:?} ({})",
                text.trim(),
                data.len()
            );
            let event = UartDataEvent {
                channel: 2,
                data,
            };
            if let Err(e) = zmq_pub.publish_uart_data(&event) {
                error!("ZMQ publish failed: {e}");
            }
        }

        let seq = shm.sequence();
        if seq == last_seq {
            continue;
        }
        last_seq = seq;

        // Check each monitored GPIO port for changes
        for entry in gpio_cached.iter_mut() {
            let (port_idx, port_label, ref mut prev) = *entry;
            let current = shm.gpio_port(port_idx);

            if current == *prev {
                continue;
            }

            let changed = prev.changed_pins(&current);

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
