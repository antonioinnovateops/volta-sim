use super::layout::*;

/// Snapshot of one ADC channel's state from SHM.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AdcChannelState {
    pub raw_value: u16,
    pub voltage: f32,
    pub enabled: bool,
    pub sample_rate: u32,
}

impl AdcChannelState {
    /// Read an ADC channel's state from the SHM slice.
    pub fn from_shm(data: &[u8], channel: usize) -> Self {
        let base = ADC_OFFSET + channel * ADC_CHANNEL_SIZE;
        Self {
            raw_value: u16::from_le_bytes(
                data[base + ADC_RAW_VALUE..base + ADC_RAW_VALUE + 2]
                    .try_into()
                    .unwrap(),
            ),
            voltage: f32::from_le_bytes(
                data[base + ADC_VOLTAGE..base + ADC_VOLTAGE + 4]
                    .try_into()
                    .unwrap(),
            ),
            enabled: data[base + ADC_ENABLED] != 0,
            sample_rate: u32::from_le_bytes(
                data[base + ADC_SAMPLE_RATE..base + ADC_SAMPLE_RATE + 4]
                    .try_into()
                    .unwrap(),
            ),
        }
    }
}
