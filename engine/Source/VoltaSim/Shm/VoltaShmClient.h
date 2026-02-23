#pragma once

#include "CoreMinimal.h"
#include "VoltaShmLayout.h"

/**
 * POSIX shared memory client for reading/writing Volta SHM.
 * Thread-safe for single-writer-per-field usage (UE5 writes input_state, Renode writes output_state).
 */
class VOLTASIM_API FVoltaShmClient
{
public:
	FVoltaShmClient();
	~FVoltaShmClient();

	/** Open the SHM file. Returns true on success. */
	bool Open(const FString& Path);

	/** Close and unmap the SHM region. */
	void Close();

	/** Whether the SHM is open and the header magic/version is valid. */
	bool IsValid() const;

	/** Read a single GPIO output pin state (written by Renode). */
	bool ReadGpioOutputPin(int32 PortIndex, int32 PinIndex) const;

	/** Read the full 16-bit output_state bitmask for a port. */
	uint16 ReadGpioOutputPort(int32 PortIndex) const;

	/** Write a single GPIO input pin state (for Renode to read). */
	void WriteGpioInputPin(int32 PortIndex, int32 PinIndex, bool Value);

	/** Read the SHM sequence counter. */
	int64 ReadSequence() const;

	// ---- PWM methods ----

	/** Read PWM duty cycle (0.0 - 1.0) for a channel. */
	float ReadPwmDutyCycle(int32 Channel) const;

	/** Read PWM frequency in Hz for a channel. */
	uint32 ReadPwmFrequency(int32 Channel) const;

	/** Check if a PWM channel is enabled. */
	bool IsPwmEnabled(int32 Channel) const;

	// ---- ADC methods ----

	/** Read ADC raw value (0-4095) for a channel. */
	uint16 ReadAdcRawValue(int32 Channel) const;

	/** Read ADC voltage for a channel. */
	float ReadAdcVoltage(int32 Channel) const;

	/** Write an ADC value to SHM (for sensor injection). */
	void WriteAdcValue(int32 Channel, uint16 RawValue, float Voltage);

private:
	/** Pointer to the mmap'd SHM region. Volatile to prevent compiler caching across frames. */
	volatile uint8* ShmBase;

	/** File descriptor for the SHM file. */
	int32 ShmFd;

	/** Whether the SHM has been successfully opened and validated. */
	bool bIsValid;

	/** Validate header magic and version. */
	bool ValidateHeader() const;
};
