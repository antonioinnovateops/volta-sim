use super::layout::*;

/// Snapshot of one PWM channel's state from SHM.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct PwmChannelState {
    pub duty_cycle: f32,
    pub frequency: u32,
    pub enabled: bool,
    pub polarity: u8,
    pub period_us: u32,
}

impl PwmChannelState {
    /// Read a PWM channel's state from the SHM slice.
    pub fn from_shm(data: &[u8], channel: usize) -> Self {
        let base = PWM_OFFSET + channel * PWM_CHANNEL_SIZE;
        Self {
            duty_cycle: f32::from_le_bytes(
                data[base + PWM_DUTY_CYCLE..base + PWM_DUTY_CYCLE + 4]
                    .try_into()
                    .unwrap(),
            ),
            frequency: u32::from_le_bytes(
                data[base + PWM_FREQUENCY..base + PWM_FREQUENCY + 4]
                    .try_into()
                    .unwrap(),
            ),
            enabled: data[base + PWM_ENABLED] != 0,
            polarity: data[base + PWM_POLARITY],
            period_us: u32::from_le_bytes(
                data[base + PWM_PERIOD_US..base + PWM_PERIOD_US + 4]
                    .try_into()
                    .unwrap(),
            ),
        }
    }
}
