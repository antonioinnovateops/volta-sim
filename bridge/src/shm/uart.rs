// UART shared memory access — stub for Sprint 2.
// Will be implemented in Sprint 4 when the API/WebSocket layer is added.

#![allow(dead_code)]

use super::layout::UART_OFFSET;

pub const UART_CHANNEL_SIZE: usize = 1024;
pub const UART_CHANNEL_COUNT: usize = 8;

/// Placeholder for UART channel state.
#[derive(Debug, Clone)]
pub struct UartChannelState {
    pub channel: usize,
}

impl UartChannelState {
    pub fn from_shm(_data: &[u8], channel: usize) -> Self {
        let _base = UART_OFFSET + channel * UART_CHANNEL_SIZE;
        Self { channel }
    }
}
