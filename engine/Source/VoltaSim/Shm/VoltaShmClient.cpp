#include "VoltaShmClient.h"

#if PLATFORM_LINUX
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

FVoltaShmClient::FVoltaShmClient()
	: ShmBase(nullptr)
	, ShmFd(-1)
	, bIsValid(false)
{
}

FVoltaShmClient::~FVoltaShmClient()
{
	Close();
}

bool FVoltaShmClient::Open(const FString& Path)
{
	Close();

#if PLATFORM_LINUX
	FString NarrowPath = Path;
	ShmFd = open(TCHAR_TO_UTF8(*NarrowPath), O_RDWR, 0);
	if (ShmFd < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("VoltaShmClient: Failed to open SHM at %s (errno=%d)"), *Path, errno);
		return false;
	}

	// Verify the file is large enough
	struct stat st;
	if (fstat(ShmFd, &st) < 0 || st.st_size < VoltaShm::ShmSize)
	{
		UE_LOG(LogTemp, Warning, TEXT("VoltaShmClient: SHM file too small (%lld < %d)"),
			(long long)st.st_size, VoltaShm::ShmSize);
		close(ShmFd);
		ShmFd = -1;
		return false;
	}

	void* Mapped = mmap(nullptr, VoltaShm::ShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, ShmFd, 0);
	if (Mapped == MAP_FAILED)
	{
		UE_LOG(LogTemp, Warning, TEXT("VoltaShmClient: mmap failed (errno=%d)"), errno);
		close(ShmFd);
		ShmFd = -1;
		return false;
	}

	ShmBase = static_cast<volatile uint8*>(Mapped);

	if (!ValidateHeader())
	{
		UE_LOG(LogTemp, Warning, TEXT("VoltaShmClient: Invalid SHM header (bad magic or version)"));
		munmap(const_cast<uint8*>(const_cast<volatile uint8*>(ShmBase)), VoltaShm::ShmSize);
		ShmBase = nullptr;
		close(ShmFd);
		ShmFd = -1;
		return false;
	}

	bIsValid = true;
	UE_LOG(LogTemp, Log, TEXT("VoltaShmClient: Opened SHM at %s"), *Path);
	return true;
#else
	UE_LOG(LogTemp, Error, TEXT("VoltaShmClient: Only supported on Linux"));
	return false;
#endif
}

void FVoltaShmClient::Close()
{
#if PLATFORM_LINUX
	if (ShmBase)
	{
		munmap(const_cast<uint8*>(const_cast<volatile uint8*>(ShmBase)), VoltaShm::ShmSize);
		ShmBase = nullptr;
	}
	if (ShmFd >= 0)
	{
		close(ShmFd);
		ShmFd = -1;
	}
#endif
	bIsValid = false;
}

bool FVoltaShmClient::IsValid() const
{
	return bIsValid && ShmBase != nullptr;
}

bool FVoltaShmClient::ReadGpioOutputPin(int32 PortIndex, int32 PinIndex) const
{
	if (!IsValid() || PortIndex < 0 || PortIndex >= VoltaShm::GpioPortCount || PinIndex < 0 || PinIndex > 15)
	{
		return false;
	}

	const int32 Offset = VoltaShm::GpioOffset + PortIndex * VoltaShm::GpioPortSize + VoltaShm::GpioOutputState;

	// Volatile read to ensure we get the latest value from SHM each frame
	const volatile uint8* Ptr = ShmBase + Offset;
	uint16 OutputState = static_cast<uint16>(Ptr[0]) | (static_cast<uint16>(Ptr[1]) << 8);

	return (OutputState & (1 << PinIndex)) != 0;
}

uint16 FVoltaShmClient::ReadGpioOutputPort(int32 PortIndex) const
{
	if (!IsValid() || PortIndex < 0 || PortIndex >= VoltaShm::GpioPortCount)
	{
		return 0;
	}

	const int32 Offset = VoltaShm::GpioOffset + PortIndex * VoltaShm::GpioPortSize + VoltaShm::GpioOutputState;
	const volatile uint8* Ptr = ShmBase + Offset;
	return static_cast<uint16>(Ptr[0]) | (static_cast<uint16>(Ptr[1]) << 8);
}

void FVoltaShmClient::WriteGpioInputPin(int32 PortIndex, int32 PinIndex, bool Value)
{
	if (!IsValid() || PortIndex < 0 || PortIndex >= VoltaShm::GpioPortCount || PinIndex < 0 || PinIndex > 15)
	{
		return;
	}

	const int32 Offset = VoltaShm::GpioOffset + PortIndex * VoltaShm::GpioPortSize + VoltaShm::GpioInputState;
	volatile uint8* Ptr = ShmBase + Offset;

	// Read-modify-write the input_state u16
	uint16 Current = static_cast<uint16>(Ptr[0]) | (static_cast<uint16>(Ptr[1]) << 8);

	if (Value)
	{
		Current |= static_cast<uint16>(1 << PinIndex);
	}
	else
	{
		Current &= static_cast<uint16>(~(1 << PinIndex));
	}

	Ptr[0] = static_cast<uint8>(Current & 0xFF);
	Ptr[1] = static_cast<uint8>((Current >> 8) & 0xFF);

	// Increment sequence counter to signal change
	volatile uint8* SeqPtr = ShmBase + VoltaShm::SequenceOffset;
	int64 Seq = 0;
	FMemory::Memcpy(&Seq, const_cast<const uint8*>(const_cast<volatile uint8*>(SeqPtr)), sizeof(int64));
	Seq++;
	FMemory::Memcpy(const_cast<uint8*>(const_cast<volatile uint8*>(SeqPtr)), &Seq, sizeof(int64));
}

int64 FVoltaShmClient::ReadSequence() const
{
	if (!IsValid())
	{
		return -1;
	}

	const volatile uint8* Ptr = ShmBase + VoltaShm::SequenceOffset;
	int64 Seq = 0;
	FMemory::Memcpy(&Seq, const_cast<const uint8*>(const_cast<volatile uint8*>(Ptr)), sizeof(int64));
	return Seq;
}

// ---- PWM methods ----

float FVoltaShmClient::ReadPwmDutyCycle(int32 Channel) const
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::PwmChannelCount)
	{
		return 0.0f;
	}

	const int32 Offset = VoltaShm::PwmOffset + Channel * VoltaShm::PwmChannelSize + VoltaShm::PwmDutyCycle;
	const volatile uint8* Ptr = ShmBase + Offset;

	// Read f32 byte-by-byte (volatile)
	uint32 Raw = static_cast<uint32>(Ptr[0])
		| (static_cast<uint32>(Ptr[1]) << 8)
		| (static_cast<uint32>(Ptr[2]) << 16)
		| (static_cast<uint32>(Ptr[3]) << 24);

	float Result;
	FMemory::Memcpy(&Result, &Raw, sizeof(float));
	return Result;
}

uint32 FVoltaShmClient::ReadPwmFrequency(int32 Channel) const
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::PwmChannelCount)
	{
		return 0;
	}

	const int32 Offset = VoltaShm::PwmOffset + Channel * VoltaShm::PwmChannelSize + VoltaShm::PwmFrequency;
	const volatile uint8* Ptr = ShmBase + Offset;
	return static_cast<uint32>(Ptr[0])
		| (static_cast<uint32>(Ptr[1]) << 8)
		| (static_cast<uint32>(Ptr[2]) << 16)
		| (static_cast<uint32>(Ptr[3]) << 24);
}

bool FVoltaShmClient::IsPwmEnabled(int32 Channel) const
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::PwmChannelCount)
	{
		return false;
	}

	const int32 Offset = VoltaShm::PwmOffset + Channel * VoltaShm::PwmChannelSize + VoltaShm::PwmEnabled;
	return ShmBase[Offset] != 0;
}

// ---- ADC methods ----

uint16 FVoltaShmClient::ReadAdcRawValue(int32 Channel) const
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::AdcChannelCount)
	{
		return 0;
	}

	const int32 Offset = VoltaShm::AdcOffset + Channel * VoltaShm::AdcChannelSize + VoltaShm::AdcRawValue;
	const volatile uint8* Ptr = ShmBase + Offset;
	return static_cast<uint16>(Ptr[0]) | (static_cast<uint16>(Ptr[1]) << 8);
}

float FVoltaShmClient::ReadAdcVoltage(int32 Channel) const
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::AdcChannelCount)
	{
		return 0.0f;
	}

	const int32 Offset = VoltaShm::AdcOffset + Channel * VoltaShm::AdcChannelSize + VoltaShm::AdcVoltage;
	const volatile uint8* Ptr = ShmBase + Offset;

	uint32 Raw = static_cast<uint32>(Ptr[0])
		| (static_cast<uint32>(Ptr[1]) << 8)
		| (static_cast<uint32>(Ptr[2]) << 16)
		| (static_cast<uint32>(Ptr[3]) << 24);

	float Result;
	FMemory::Memcpy(&Result, &Raw, sizeof(float));
	return Result;
}

void FVoltaShmClient::WriteAdcValue(int32 Channel, uint16 RawValue, float Voltage)
{
	if (!IsValid() || Channel < 0 || Channel >= VoltaShm::AdcChannelCount)
	{
		return;
	}

	const int32 BaseOffset = VoltaShm::AdcOffset + Channel * VoltaShm::AdcChannelSize;

	// Write raw_value u16
	volatile uint8* RawPtr = ShmBase + BaseOffset + VoltaShm::AdcRawValue;
	RawPtr[0] = static_cast<uint8>(RawValue & 0xFF);
	RawPtr[1] = static_cast<uint8>((RawValue >> 8) & 0xFF);

	// Write voltage f32
	volatile uint8* VoltPtr = ShmBase + BaseOffset + VoltaShm::AdcVoltage;
	uint32 VoltRaw;
	FMemory::Memcpy(&VoltRaw, &Voltage, sizeof(uint32));
	VoltPtr[0] = static_cast<uint8>(VoltRaw & 0xFF);
	VoltPtr[1] = static_cast<uint8>((VoltRaw >> 8) & 0xFF);
	VoltPtr[2] = static_cast<uint8>((VoltRaw >> 16) & 0xFF);
	VoltPtr[3] = static_cast<uint8>((VoltRaw >> 24) & 0xFF);

	// Set enabled
	volatile uint8* EnPtr = ShmBase + BaseOffset + VoltaShm::AdcEnabled;
	*EnPtr = 1;

	// Increment sequence counter
	volatile uint8* SeqPtr = ShmBase + VoltaShm::SequenceOffset;
	int64 Seq = 0;
	FMemory::Memcpy(&Seq, const_cast<const uint8*>(const_cast<volatile uint8*>(SeqPtr)), sizeof(int64));
	Seq++;
	FMemory::Memcpy(const_cast<uint8*>(const_cast<volatile uint8*>(SeqPtr)), &Seq, sizeof(int64));
}

bool FVoltaShmClient::ValidateHeader() const
{
	if (!ShmBase)
	{
		return false;
	}

	const volatile uint8* MagicPtr = ShmBase + VoltaShm::MagicOffset;
	uint32 ReadMagic = static_cast<uint32>(MagicPtr[0])
		| (static_cast<uint32>(MagicPtr[1]) << 8)
		| (static_cast<uint32>(MagicPtr[2]) << 16)
		| (static_cast<uint32>(MagicPtr[3]) << 24);

	const volatile uint8* VerPtr = ShmBase + VoltaShm::VersionOffset;
	uint32 ReadVersion = static_cast<uint32>(VerPtr[0])
		| (static_cast<uint32>(VerPtr[1]) << 8)
		| (static_cast<uint32>(VerPtr[2]) << 16)
		| (static_cast<uint32>(VerPtr[3]) << 24);

	return ReadMagic == VoltaShm::Magic && ReadVersion == VoltaShm::Version;
}
