#include "VoltaBoardActor.h"
#include "Actors/VoltaLEDActor.h"
#include "Actors/VoltaButtonActor.h"
#include "Actors/VoltaMotorActor.h"
#include "Shm/VoltaShmLayout.h"

AVoltaBoardActor::AVoltaBoardActor()
	: bShmConnected(false)
	, RetryTimer(0.0f)
	, RetryCount(0)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create a simple root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("BoardRoot"));
	RootComponent = Root;
}

void AVoltaBoardActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("VoltaBoard: Starting, SHM path = %s"), *ShmPath);

	// Attempt initial SHM connection
	if (TryOpenShm())
	{
		SpawnBoardComponents();
	}
}

void AVoltaBoardActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShmClient.Close();
	Super::EndPlay(EndPlayReason);
}

void AVoltaBoardActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Retry SHM connection if not yet connected
	if (!bShmConnected)
	{
		RetryTimer -= DeltaTime;
		if (RetryTimer <= 0.0f)
		{
			RetryTimer = ShmRetryInterval;

			if (TryOpenShm())
			{
				SpawnBoardComponents();
			}
			else
			{
				RetryCount++;
				if (MaxRetries > 0 && RetryCount >= MaxRetries)
				{
					UE_LOG(LogTemp, Error, TEXT("VoltaBoard: Failed to open SHM after %d retries"), RetryCount);
					SetActorTickEnabled(false);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("VoltaBoard: SHM not ready, retry %d/%d..."),
						RetryCount, MaxRetries > 0 ? MaxRetries : -1);
				}
			}
		}
	}
}

bool AVoltaBoardActor::TryOpenShm()
{
	if (ShmClient.Open(ShmPath))
	{
		bShmConnected = true;
		UE_LOG(LogTemp, Log, TEXT("VoltaBoard: SHM connected (seq=%lld)"), ShmClient.ReadSequence());
		return true;
	}
	return false;
}

void AVoltaBoardActor::SpawnBoardComponents()
{
	UE_LOG(LogTemp, Log, TEXT("VoltaBoard: Spawning STM32F4 Discovery board components"));

	// STM32F4 Discovery LED layout (Port D):
	//   PD12 = Green  (top)
	//   PD13 = Orange (right)
	//   PD14 = Red    (bottom)
	//   PD15 = Blue   (left)
	// Arranged in a compass pattern around the board center

	const float LedSpacing = 40.0f; // cm

	LEDs.Add(SpawnLED(VoltaShm::PortD, VoltaShm::PinGreenLed,
		FLinearColor::Green, FVector(0.0f, LedSpacing, 0.0f)));

	LEDs.Add(SpawnLED(VoltaShm::PortD, VoltaShm::PinOrangeLed,
		FLinearColor(1.0f, 0.5f, 0.0f), FVector(LedSpacing, 0.0f, 0.0f)));

	LEDs.Add(SpawnLED(VoltaShm::PortD, VoltaShm::PinRedLed,
		FLinearColor::Red, FVector(0.0f, -LedSpacing, 0.0f)));

	LEDs.Add(SpawnLED(VoltaShm::PortD, VoltaShm::PinBlueLed,
		FLinearColor::Blue, FVector(-LedSpacing, 0.0f, 0.0f)));

	// User button (Port A, Pin 0) - positioned below the LEDs
	Buttons.Add(SpawnButton(VoltaShm::PortA, VoltaShm::PinUserButton,
		FVector(0.0f, -LedSpacing * 2.0f, 0.0f)));

	// Motor (PWM channel 0 = TIM1 CH1) - positioned to the right of the board
	Motors.Add(SpawnMotor(0, FVector(LedSpacing * 2.0f, 0.0f, 0.0f)));

	UE_LOG(LogTemp, Log, TEXT("VoltaBoard: Spawned %d LEDs, %d buttons, %d motors"),
		LEDs.Num(), Buttons.Num(), Motors.Num());

	// Disable tick once connected - LEDs/buttons handle their own ticking
	SetActorTickEnabled(false);
}

AVoltaLEDActor* AVoltaBoardActor::SpawnLED(int32 PortIdx, int32 PinIdx, FLinearColor Color, FVector Offset)
{
	FVector SpawnLoc = GetActorLocation() + Offset;
	FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVoltaLEDActor* Led = GetWorld()->SpawnActor<AVoltaLEDActor>(AVoltaLEDActor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
	if (Led)
	{
		Led->OnColor = Color;
		Led->Initialize(&ShmClient, PortIdx, PinIdx);
	}
	return Led;
}

AVoltaButtonActor* AVoltaBoardActor::SpawnButton(int32 PortIdx, int32 PinIdx, FVector Offset)
{
	FVector SpawnLoc = GetActorLocation() + Offset;
	FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVoltaButtonActor* Btn = GetWorld()->SpawnActor<AVoltaButtonActor>(AVoltaButtonActor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
	if (Btn)
	{
		Btn->Initialize(&ShmClient, PortIdx, PinIdx);
	}
	return Btn;
}

AVoltaMotorActor* AVoltaBoardActor::SpawnMotor(int32 PwmChannel, FVector Offset)
{
	FVector SpawnLoc = GetActorLocation() + Offset;
	FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVoltaMotorActor* Motor = GetWorld()->SpawnActor<AVoltaMotorActor>(AVoltaMotorActor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
	if (Motor)
	{
		Motor->Initialize(&ShmClient, PwmChannel);
	}
	return Motor;
}
