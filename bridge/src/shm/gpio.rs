use super::layout::*;

/// Snapshot of one GPIO port's state from SHM.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GpioPortState {
    pub output_state: u16,
    pub input_state: u16,
}

impl GpioPortState {
    /// Read a GPIO port's state from the SHM slice.
    pub fn from_shm(data: &[u8], port_index: usize) -> Self {
        let base = GPIO_OFFSET + port_index * GPIO_PORT_SIZE;
        Self {
            output_state: u16::from_le_bytes(
                data[base + GPIO_OUTPUT_STATE..base + GPIO_OUTPUT_STATE + 2]
                    .try_into()
                    .unwrap(),
            ),
            input_state: u16::from_le_bytes(
                data[base + GPIO_INPUT_STATE..base + GPIO_INPUT_STATE + 2]
                    .try_into()
                    .unwrap(),
            ),
        }
    }

    /// Check if a specific pin is HIGH in the output state.
    pub fn pin_output(&self, pin: u8) -> bool {
        self.output_state & (1 << pin) != 0
    }

    /// Return bitmask of pins that differ between two states.
    pub fn changed_pins(&self, other: &Self) -> u16 {
        self.output_state ^ other.output_state
    }
}

/// Human-readable port letter from index.
pub fn port_letter(index: usize) -> char {
    (b'A' + index as u8) as char
}
