use anyhow::{Context, Result};
use serde::Serialize;

/// A GPIO state change event published over ZMQ.
#[derive(Debug, Serialize)]
pub struct GpioChangeEvent {
    pub port: String,
    pub pin: u8,
    pub state: bool,
    pub sequence: u64,
}

/// ZMQ PUB socket for broadcasting peripheral events.
pub struct ZmqPublisher {
    socket: zmq::Socket,
}

impl ZmqPublisher {
    pub fn bind(addr: &str) -> Result<Self> {
        let ctx = zmq::Context::new();
        let socket = ctx.socket(zmq::PUB)?;
        socket
            .bind(addr)
            .with_context(|| format!("Failed to bind ZMQ PUB to {addr}"))?;
        Ok(Self { socket })
    }

    /// Publish a GPIO change event as a two-part ZMQ message:
    /// part 1 = topic string, part 2 = MessagePack payload.
    pub fn publish_gpio_change(&self, event: &GpioChangeEvent) -> Result<()> {
        let topic = "volta.gpio.change";
        let payload = rmp_serde::to_vec(event)?;
        self.socket.send(topic.as_bytes(), zmq::SNDMORE)?;
        self.socket.send(&payload, 0)?;
        Ok(())
    }
}
