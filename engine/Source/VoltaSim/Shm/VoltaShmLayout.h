#pragma once

// SHM layout constants — must match bridge/src/shm/layout.rs and VoltaGPIOBridge.cs exactly.
//
// Memory Map (65536 bytes total):
// Offset  Size   Description
// 0x0000  64     Header (magic, version, sequence, flags)
// 0x0040  256    GPIO banks (16 ports x 16 bytes)
// 0x0140  8192   UART buffers (8 channels x 1024 bytes)
// 0x2140  256    PWM channels (16 channels x 16 bytes)
// 0x2240  256    ADC channels (16 channels x 16 bytes)

namespace VoltaShm
{
	constexpr int32 ShmSize = 65536;
	constexpr uint32 Magic = 0x564F4C54; // "VOLT"
	constexpr uint32 Version = 1;

	// Header offsets
	constexpr int32 HeaderOffset = 0x0000;
	constexpr int32 HeaderSize = 64;
	constexpr int32 MagicOffset = 0;
	constexpr int32 VersionOffset = 4;
	constexpr int32 SequenceOffset = 8;
	constexpr int32 FlagsOffset = 16;

	// GPIO section
	constexpr int32 GpioOffset = 0x0040;
	constexpr int32 GpioPortSize = 16;    // bytes per port
	constexpr int32 GpioPortCount = 16;

	// Per-port field offsets (relative to port start)
	constexpr int32 GpioOutputState = 0;  // u16: bitmask of output pin states
	constexpr int32 GpioInputState = 2;   // u16: bitmask of input pin states

	// UART section
	constexpr int32 UartOffset = 0x0140;
	constexpr int32 UartChannelSize = 1024;
	constexpr int32 UartChannelCount = 8;
	constexpr int32 UartHeaderSize = 8;   // write_head(4) + reserved(4)
	constexpr int32 UartBufSize = 1016;   // UartChannelSize - UartHeaderSize

	// Port index mapping
	constexpr int32 PortA = 0;
	constexpr int32 PortB = 1;
	constexpr int32 PortC = 2;
	constexpr int32 PortD = 3;

	// PWM section
	constexpr int32 PwmOffset = 0x2140;
	constexpr int32 PwmChannelSize = 16;   // bytes per channel
	constexpr int32 PwmChannelCount = 16;

	// Per-channel PWM field offsets (relative to channel start)
	constexpr int32 PwmDutyCycle = 0;      // f32 (0.0-1.0)
	constexpr int32 PwmFrequency = 4;      // u32 (Hz)
	constexpr int32 PwmEnabled = 8;        // u8
	constexpr int32 PwmPolarity = 9;       // u8 (0=ACTIVE_HIGH)
	constexpr int32 PwmPeriodUs = 12;      // u32

	// ADC section
	constexpr int32 AdcOffset = 0x2240;
	constexpr int32 AdcChannelSize = 16;   // bytes per channel
	constexpr int32 AdcChannelCount = 16;

	// Per-channel ADC field offsets (relative to channel start)
	constexpr int32 AdcRawValue = 0;       // u16 (0-4095)
	constexpr int32 AdcVoltage = 2;        // f32 (byte-by-byte)
	constexpr int32 AdcEnabled = 6;        // u8
	constexpr int32 AdcSampleRate = 8;     // u32

	// STM32F4 Discovery LED pins (on Port D)
	constexpr int32 PinGreenLed = 12;
	constexpr int32 PinOrangeLed = 13;
	constexpr int32 PinRedLed = 14;
	constexpr int32 PinBlueLed = 15;

	// STM32F4 Discovery user button (on Port A)
	constexpr int32 PinUserButton = 0;

	// Default SHM path
	constexpr const TCHAR* DefaultShmPath = TEXT("/dev/shm/volta_peripheral_state");
}
