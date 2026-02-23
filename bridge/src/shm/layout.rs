/// SHM layout constants — must match VoltaGPIOBridge.cs exactly.
///
/// ```text
/// Offset  Size   Description
/// 0x0000  64     Header (magic, version, sequence, flags)
/// 0x0040  256    GPIO banks (16 ports × 16 bytes)
/// 0x0140  8192   UART buffers (8 channels × 1024 bytes)
/// 0x2140  256    PWM channels (16 channels × 16 bytes)
/// 0x2240  256    ADC channels (16 channels × 16 bytes)
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

// UART section
pub const UART_OFFSET: usize = 0x0140;
pub const UART_CHANNEL_SIZE: usize = 1024;
pub const UART_CHANNEL_COUNT: usize = 8;
pub const UART_HEADER_SIZE: usize = 8; // write_head(4) + reserved(4)
pub const UART_BUF_SIZE: usize = 1016; // UART_CHANNEL_SIZE - UART_HEADER_SIZE

// PWM section
pub const PWM_OFFSET: usize = 0x2140;
pub const PWM_CHANNEL_SIZE: usize = 16;
pub const PWM_CHANNEL_COUNT: usize = 16;

// Per-channel PWM field offsets (relative to channel start)
pub const PWM_DUTY_CYCLE: usize = 0;  // f32 (0.0-1.0)
pub const PWM_FREQUENCY: usize = 4;   // u32 (Hz)
pub const PWM_ENABLED: usize = 8;     // u8
pub const PWM_POLARITY: usize = 9;    // u8 (0=ACTIVE_HIGH)
pub const PWM_PERIOD_US: usize = 12;  // u32

// ADC section
pub const ADC_OFFSET: usize = 0x2240;
pub const ADC_CHANNEL_SIZE: usize = 16;
pub const ADC_CHANNEL_COUNT: usize = 16;

// Per-channel ADC field offsets (relative to channel start)
pub const ADC_RAW_VALUE: usize = 0;    // u16 (0-4095)
pub const ADC_VOLTAGE: usize = 2;      // f32 (byte-by-byte)
pub const ADC_ENABLED: usize = 6;      // u8
pub const ADC_SAMPLE_RATE: usize = 8;  // u32

// Port index mapping
pub const PORT_A: usize = 0;
pub const PORT_B: usize = 1;
pub const PORT_C: usize = 2;
pub const PORT_D: usize = 3;
