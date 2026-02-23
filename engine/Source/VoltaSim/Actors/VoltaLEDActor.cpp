#include "VoltaLEDActor.h"
#include "Shm/VoltaShmClient.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVoltaLEDActor::AVoltaLEDActor()
	: ShmClient(nullptr)
	, bLastState(false)
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LEDMesh"));
	RootComponent = MeshComp;

	// Use engine's built-in sphere mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(SphereMesh.Object);
	}

	MeshComp->SetWorldScale3D(FVector(0.3f, 0.3f, 0.3f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AVoltaLEDActor::Initialize(FVoltaShmClient* InShmClient, int32 InPortIndex, int32 InPinIndex)
{
	ShmClient = InShmClient;
	PortIndex = InPortIndex;
	PinIndex = InPinIndex;
}

void AVoltaLEDActor::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material instance for emissive color changes
	UMaterialInterface* BaseMat = MeshComp->GetMaterial(0);
	if (BaseMat)
	{
		DynMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		MeshComp->SetMaterial(0, DynMaterial);
	}

	// Start in OFF state
	UpdateLEDState(false);
}

void AVoltaLEDActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ShmClient || !ShmClient->IsValid())
	{
		return;
	}

	bool bCurrentState = ShmClient->ReadGpioOutputPin(PortIndex, PinIndex);

	// Only update material when state actually changes
	if (bCurrentState != bLastState)
	{
		UpdateLEDState(bCurrentState);
		bLastState = bCurrentState;
	}
}

void AVoltaLEDActor::UpdateLEDState(bool bOn)
{
	if (!DynMaterial)
	{
		return;
	}

	FLinearColor Color = bOn ? OnColor * EmissiveIntensity : OffColor;
	DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), Color);
	DynMaterial->SetVectorParameterValue(FName("BaseColor"), bOn ? OnColor : OffColor);
}
