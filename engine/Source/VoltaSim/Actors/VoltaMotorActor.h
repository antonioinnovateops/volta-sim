#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoltaMotorActor.generated.h"

class UStaticMeshComponent;
class FVoltaShmClient;

/**
 * 3D motor actor that reads PWM duty cycle from SHM each tick.
 * Renders as a cylinder that rotates proportionally to duty cycle.
 * Spawned and configured by VoltaBoardActor.
 */
UCLASS()
class VOLTASIM_API AVoltaMotorActor : public AActor
{
	GENERATED_BODY()

public:
	AVoltaMotorActor();

	/** Initialize with SHM client reference and PWM channel. */
	void Initialize(FVoltaShmClient* InShmClient, int32 InPwmChannel);

	virtual void Tick(float DeltaTime) override;

	/** PWM channel index to read duty cycle from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|PWM")
	int32 PwmChannel = 0;

	/** Maximum RPM at 100% duty cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Motor")
	float MaxRPM = 3000.0f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	UStaticMeshComponent* MeshComp;

	FVoltaShmClient* ShmClient;
	float CurrentAngle;
};
