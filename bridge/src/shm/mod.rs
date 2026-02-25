pub mod adc;
pub mod gpio;
pub mod header;
pub mod layout;
pub mod pwm;
pub mod uart;

use anyhow::{bail, Context, Result};
use memmap2::MmapMut;
use std::fs::OpenOptions;

use header::ShmHeader;
use layout::*;

/// Memory-mapped view of the Volta peripheral state SHM region.
pub struct ShmRegion {
    mmap: MmapMut,
}

impl ShmRegion {
    /// Open an existing SHM region (created by VoltaGPIOBridge.cs in Renode).
    pub fn open(path: &str, expected_size: usize) -> Result<Self> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .open(path)
            .with_context(|| format!("Failed to open SHM at {path}"))?;

        let metadata = file.metadata()?;
        if (metadata.len() as usize) < expected_size {
            bail!(
                "SHM file too small: {} bytes (expected >= {})",
                metadata.len(),
                expected_size
            );
        }

        let mmap = unsafe { MmapMut::map_mut(&file)? };
        let region = Self { mmap };

        let hdr = region.header();
        if !hdr.is_valid() {
            bail!(
                "SHM header invalid: magic=0x{:08X} version={} initialized={}",
                hdr.magic(),
                hdr.version(),
                hdr.is_initialized()
            );
        }

        Ok(region)
    }

    /// Try to open, retrying until the SHM is initialized or timeout.
    pub fn open_with_retry(path: &str, size: usize, max_attempts: u32) -> Result<Self> {
        for attempt in 1..=max_attempts {
            match Self::open(path, size) {
                Ok(region) => return Ok(region),
                Err(e) => {
                    if attempt == max_attempts {
                        return Err(e);
                    }
                    tracing::debug!("SHM not ready (attempt {attempt}/{max_attempts}): {e}");
                    std::thread::sleep(std::time::Duration::from_millis(500));
                }
            }
        }
        unreachable!()
    }

    pub fn header(&self) -> ShmHeader<'_> {
        ShmHeader::new(&self.mmap[..HEADER_SIZE])
    }

    pub fn sequence(&self) -> u64 {
        u64::from_le_bytes(
            self.mmap[SEQUENCE_OFFSET..SEQUENCE_OFFSET + 8]
                .try_into()
                .unwrap(),
        )
    }

    pub fn data(&self) -> &[u8] {
        &self.mmap
    }

    pub fn gpio_port(&self, port_index: usize) -> gpio::GpioPortState {
        gpio::GpioPortState::from_shm(&self.mmap, port_index)
    }

    /// Write a value to the GPIO input_state field (for button simulation).
    pub fn write_gpio_input(&mut self, port_index: usize, pin: u8, value: bool) {
        let base = GPIO_OFFSET + port_index * GPIO_PORT_SIZE + GPIO_INPUT_STATE;
        let mut current =
            u16::from_le_bytes(self.mmap[base..base + 2].try_into().unwrap());
        if value {
            current |= 1 << pin;
        } else {
            current &= !(1 << pin);
        }
        self.mmap[base..base + 2].copy_from_slice(&current.to_le_bytes());
    }

    pub fn pwm_channel(&self, channel: usize) -> pwm::PwmChannelState {
        pwm::PwmChannelState::from_shm(&self.mmap, channel)
    }

    pub fn adc_channel(&self, channel: usize) -> adc::AdcChannelState {
        adc::AdcChannelState::from_shm(&self.mmap, channel)
    }

    /// Write UART data to SHM ring buffer (received from Renode socket terminal).
    pub fn write_uart_data(&mut self, channel: usize, data: &[u8]) {
        let base = UART_OFFSET + channel * UART_CHANNEL_SIZE;

        // Read current write_head (u32 LE at base)
        let write_head = u32::from_le_bytes(
            self.mmap[base..base + 4].try_into().unwrap(),
        );

        // Write each byte into the ring buffer
        let data_base = base + UART_HEADER_SIZE;
        let mut head = write_head;
        for &byte in data {
            let idx = data_base + (head as usize % UART_BUF_SIZE);
            self.mmap[idx] = byte;
            head = head.wrapping_add(1);
        }

        // Update write_head
        self.mmap[base..base + 4].copy_from_slice(&head.to_le_bytes());
    }

    /// Write an ADC value to SHM (for sensor injection from API/UE5).
    pub fn write_adc_value(&mut self, channel: usize, raw_value: u16, voltage: f32) {
        let base = ADC_OFFSET + channel * ADC_CHANNEL_SIZE;

        // raw_value u16 at +0x00
        self.mmap[base + ADC_RAW_VALUE..base + ADC_RAW_VALUE + 2]
            .copy_from_slice(&raw_value.to_le_bytes());

        // voltage f32 at +0x02
        self.mmap[base + ADC_VOLTAGE..base + ADC_VOLTAGE + 4]
            .copy_from_slice(&voltage.to_le_bytes());

        // enabled u8 at +0x06
        self.mmap[base + ADC_ENABLED] = 1;
    }
}
