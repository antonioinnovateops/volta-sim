/// SHM layout constants — must match VoltaGPIOBridge.cs exactly.
///
/// ```text
/// Offset  Size   Description
/// 0x0000  64     Header (magic, version, sequence, flags)
/// 0x0040  256    GPIO banks (16 ports × 16 bytes)
/// 0x0140  ...    UART buffers (Sprint 4)
/// ```

pub const SHM_SIZE: usize = 65536;
pub const MAGIC: u32 = 0x564F_4C54; // "VOLT"
pub const VERSION: u32 = 1;

// Header offsets
pub const HEADER_OFFSET: usize = 0x0000;
pub const HEADER_SIZE: usize = 64;
pub const MAGIC_OFFSET: usize = 0;
pub const VERSION_OFFSET: usize = 4;
pub const SEQUENCE_OFFSET: usize = 8;
pub const FLAGS_OFFSET: usize = 16;

// GPIO section
pub const GPIO_OFFSET: usize = 0x0040;
pub const GPIO_PORT_SIZE: usize = 16;
pub const GPIO_PORT_COUNT: usize = 16;

// Per-port field offsets (relative to port start)
pub const GPIO_OUTPUT_STATE: usize = 0;
pub const GPIO_INPUT_STATE: usize = 2;

// UART section (Sprint 4)
pub const UART_OFFSET: usize = 0x0140;

// Port index mapping
pub const PORT_A: usize = 0;
pub const PORT_B: usize = 1;
pub const PORT_C: usize = 2;
pub const PORT_D: usize = 3;
