#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoltaButtonActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class FVoltaShmClient;

UENUM(BlueprintType)
enum class EVoltaButtonMode : uint8
{
	Momentary,  // Press and release
	Toggle      // Click to toggle on/off
};

/**
 * Interactive button actor that writes GPIO input state to SHM.
 * Clickable via Pixel Streaming mouse events.
 */
UCLASS()
class VOLTASIM_API AVoltaButtonActor : public AActor
{
	GENERATED_BODY()

public:
	AVoltaButtonActor();

	/** Initialize with SHM client reference and pin configuration. */
	void Initialize(FVoltaShmClient* InShmClient, int32 InPortIndex, int32 InPinIndex);

	virtual void Tick(float DeltaTime) override;

	/** GPIO port index (0=A, 3=D, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|GPIO")
	int32 PortIndex = 0;

	/** GPIO pin index (0-15) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|GPIO")
	int32 PinIndex = 0;

	/** Button behavior mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Button")
	EVoltaButtonMode ButtonMode = EVoltaButtonMode::Momentary;

	/** Duration to hold the button in momentary mode (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Button")
	float MomentaryHoldTime = 0.2f;

	/** Color when pressed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Appearance")
	FLinearColor PressedColor = FLinearColor(0.2f, 0.5f, 1.0f);

	/** Color when released */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volta|Appearance")
	FLinearColor ReleasedColor = FLinearColor(0.3f, 0.3f, 0.3f);

	/** Called when the button is clicked (e.g., via Pixel Streaming). */
	UFUNCTION(BlueprintCallable, Category = "Volta|Button")
	void OnButtonPressed();

	/** Called when the button is released (momentary mode). */
	UFUNCTION(BlueprintCallable, Category = "Volta|Button")
	void OnButtonReleased();

protected:
	virtual void BeginPlay() override;
	virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

private:
	UPROPERTY()
	UStaticMeshComponent* MeshComp;

	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	FVoltaShmClient* ShmClient;
	bool bIsPressed;
	float MomentaryTimer;

	void SetShmState(bool bPressed);
	void UpdateVisual();
};
