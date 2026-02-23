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

/// A UART data event published over ZMQ.
#[derive(Debug, Serialize)]
pub struct UartDataEvent {
    pub channel: usize,
    pub data: Vec<u8>,
}

/// A PWM state update event published over ZMQ.
#[derive(Debug, Serialize)]
pub struct PwmUpdateEvent {
    pub channel: usize,
    pub duty_cycle: f32,
    pub frequency: u32,
    pub enabled: bool,
    pub polarity: u8,
    pub period_us: u32,
}

/// An ADC value change event published over ZMQ.
#[derive(Debug, Serialize)]
pub struct AdcChangeEvent {
    pub channel: usize,
    pub raw_value: u16,
    pub voltage: f32,
    pub enabled: bool,
    pub sample_rate: u32,
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

    /// Publish a UART data event.
    pub fn publish_uart_data(&self, event: &UartDataEvent) -> Result<()> {
        let topic = "volta.uart.data";
        let payload = rmp_serde::to_vec(event)?;
        self.socket.send(topic.as_bytes(), zmq::SNDMORE)?;
        self.socket.send(&payload, 0)?;
        Ok(())
    }

    /// Publish a PWM state update event.
    pub fn publish_pwm_update(&self, event: &PwmUpdateEvent) -> Result<()> {
        let topic = "volta.pwm.update";
        let payload = rmp_serde::to_vec(event)?;
        self.socket.send(topic.as_bytes(), zmq::SNDMORE)?;
        self.socket.send(&payload, 0)?;
        Ok(())
    }

    /// Publish an ADC value change event.
    pub fn publish_adc_change(&self, event: &AdcChangeEvent) -> Result<()> {
        let topic = "volta.adc.change";
        let payload = rmp_serde::to_vec(event)?;
        self.socket.send(topic.as_bytes(), zmq::SNDMORE)?;
        self.socket.send(&payload, 0)?;
        Ok(())
    }
}
