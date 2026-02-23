#include "VoltaButtonActor.h"
#include "Shm/VoltaShmClient.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVoltaButtonActor::AVoltaButtonActor()
	: ShmClient(nullptr)
	, bIsPressed(false)
	, MomentaryTimer(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	RootComponent = MeshComp;

	// Use engine's built-in cylinder mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(CylinderMesh.Object);
	}

	MeshComp->SetWorldScale3D(FVector(0.2f, 0.2f, 0.05f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);

	// Enable click events for Pixel Streaming interaction
	MeshComp->SetGenerateOverlapEvents(true);
}

void AVoltaButtonActor::Initialize(FVoltaShmClient* InShmClient, int32 InPortIndex, int32 InPinIndex)
{
	ShmClient = InShmClient;
	PortIndex = InPortIndex;
	PinIndex = InPinIndex;
}

void AVoltaButtonActor::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material for visual feedback
	UMaterialInterface* BaseMat = MeshComp->GetMaterial(0);
	if (BaseMat)
	{
		DynMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		MeshComp->SetMaterial(0, DynMaterial);
	}

	// Enable click events
	SetActorEnableCollision(true);

	UpdateVisual();
}

void AVoltaButtonActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle momentary release timer
	if (ButtonMode == EVoltaButtonMode::Momentary && bIsPressed && MomentaryTimer > 0.0f)
	{
		MomentaryTimer -= DeltaTime;
		if (MomentaryTimer <= 0.0f)
		{
			OnButtonReleased();
		}
	}
}

void AVoltaButtonActor::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);
	OnButtonPressed();
}

void AVoltaButtonActor::OnButtonPressed()
{
	if (ButtonMode == EVoltaButtonMode::Toggle)
	{
		bIsPressed = !bIsPressed;
	}
	else
	{
		bIsPressed = true;
		MomentaryTimer = MomentaryHoldTime;
	}

	SetShmState(bIsPressed);
	UpdateVisual();

	UE_LOG(LogTemp, Log, TEXT("VoltaButton: Port %c Pin %d -> %s"),
		'A' + PortIndex, PinIndex, bIsPressed ? TEXT("PRESSED") : TEXT("RELEASED"));
}

void AVoltaButtonActor::OnButtonReleased()
{
	if (ButtonMode == EVoltaButtonMode::Momentary)
	{
		bIsPressed = false;
		SetShmState(false);
		UpdateVisual();

		UE_LOG(LogTemp, Log, TEXT("VoltaButton: Port %c Pin %d -> RELEASED"), 'A' + PortIndex, PinIndex);
	}
}

void AVoltaButtonActor::SetShmState(bool bPressed)
{
	if (ShmClient && ShmClient->IsValid())
	{
		ShmClient->WriteGpioInputPin(PortIndex, PinIndex, bPressed);
	}
}

void AVoltaButtonActor::UpdateVisual()
{
	if (!DynMaterial)
	{
		return;
	}

	FLinearColor Color = bIsPressed ? PressedColor : ReleasedColor;
	DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
}
