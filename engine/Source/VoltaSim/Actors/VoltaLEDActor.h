#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoltaLEDActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class FVoltaShmClient;

/**
 * 3D LED actor that reads GPIO output state from SHM each tick.
 * Renders as a sphere with dynamic emissive material.
 * Spawned and configured by VoltaBoardActor.
 */
UCLASS()
class VOLTASIM_API AVoltaLEDActor : public AActor
{
	GENERATED_BODY()

public:
	AVoltaLEDActor();

	/** Initialize with SHM client reference and pin configuration. */
	void Initialize(FVoltaShmClient* InShmClient, int32 InPortIndex, int32 InPinIndex);

	virtual void Tick(float DeltaTime) override;

	/** GPIO port index (0=A, 3=D, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|GPIO")
	int32 PortIndex = 3;

	/** GPIO pin index (0-15) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|GPIO")
	int32 PinIndex = 12;

	/** Color when the LED is ON */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Appearance")
	FLinearColor OnColor = FLinearColor::Green;

	/** Color when the LED is OFF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Appearance")
	FLinearColor OffColor = FLinearColor(0.02f, 0.02f, 0.02f);

	/** Emissive intensity when ON */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Appearance")
	float EmissiveIntensity = 20.0f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	UStaticMeshComponent* MeshComp;

	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	FVoltaShmClient* ShmClient;
	bool bLastState;

	void UpdateLEDState(bool bOn);
};
