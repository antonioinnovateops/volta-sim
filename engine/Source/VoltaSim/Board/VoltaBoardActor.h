#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shm/VoltaShmClient.h"
#include "VoltaBoardActor.generated.h"

class AVoltaLEDActor;
class AVoltaButtonActor;
class AVoltaMotorActor;

/**
 * Board-level actor that owns the SHM client and spawns LEDs/buttons
 * in the STM32F4 Discovery layout.
 *
 * Retry-opens SHM with a timer to handle startup ordering
 * (Renode may not have initialized SHM yet when UE5 starts).
 */
UCLASS()
class VOLTASIM_API AVoltaBoardActor : public AActor
{
	GENERATED_BODY()

public:
	AVoltaBoardActor();

	virtual void Tick(float DeltaTime) override;

	/** Path to the SHM file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|SHM")
	FString ShmPath = VoltaShm::DefaultShmPath;

	/** Retry interval for opening SHM (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|SHM")
	float ShmRetryInterval = 1.0f;

	/** Maximum number of SHM open retries (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|SHM")
	int32 MaxRetries = 60;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FVoltaShmClient ShmClient;

	UPROPERTY()
	TArray<AVoltaLEDActor*> LEDs;

	UPROPERTY()
	TArray<AVoltaButtonActor*> Buttons;

	UPROPERTY()
	TArray<AVoltaMotorActor*> Motors;

	bool bShmConnected;
	float RetryTimer;
	int32 RetryCount;

	/** Try to open SHM. Returns true on success. */
	bool TryOpenShm();

	/** Spawn the board components (LEDs + buttons) once SHM is connected. */
	void SpawnBoardComponents();

	/** Spawn a single LED actor at the given offset from board center. */
	AVoltaLEDActor* SpawnLED(int32 PortIdx, int32 PinIdx, FLinearColor Color, FVector Offset);

	/** Spawn a single button actor at the given offset from board center. */
	AVoltaButtonActor* SpawnButton(int32 PortIdx, int32 PinIdx, FVector Offset);

	/** Spawn a single motor actor at the given offset from board center. */
	AVoltaMotorActor* SpawnMotor(int32 PwmChannel, FVector Offset);
};
