pub mod gpio;
pub mod header;
pub mod layout;
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
}
