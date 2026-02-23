use super::layout::{UART_OFFSET, UART_CHANNEL_SIZE, UART_HEADER_SIZE, UART_BUF_SIZE};

/// Reads new UART data from the SHM ring buffer for a given channel.
pub struct UartChannelReader {
    pub channel: usize,
    read_head: u32,
}

impl UartChannelReader {
    pub fn new(channel: usize) -> Self {
        Self {
            channel,
            read_head: 0,
        }
    }

    /// Read any new bytes from the ring buffer. Returns empty vec if no new data.
    pub fn read_new(&mut self, shm_data: &[u8]) -> Vec<u8> {
        let base = UART_OFFSET + self.channel * UART_CHANNEL_SIZE;

        // Read write_head (u32 little-endian at base offset)
        if base + 4 > shm_data.len() {
            return Vec::new();
        }
        let write_head = u32::from_le_bytes([
            shm_data[base],
            shm_data[base + 1],
            shm_data[base + 2],
            shm_data[base + 3],
        ]);

        if write_head == self.read_head {
            return Vec::new();
        }

        // Read new bytes from ring buffer
        let mut result = Vec::new();
        let data_base = base + UART_HEADER_SIZE;

        while self.read_head < write_head {
            let idx = data_base + (self.read_head as usize % UART_BUF_SIZE);
            if idx < shm_data.len() {
                result.push(shm_data[idx]);
            }
            self.read_head += 1;
        }

        result
    }
}
