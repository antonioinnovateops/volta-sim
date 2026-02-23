#include "VoltaMotorActor.h"
#include "Shm/VoltaShmClient.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AVoltaMotorActor::AVoltaMotorActor()
	: ShmClient(nullptr)
	, CurrentAngle(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotorMesh"));
	RootComponent = MeshComp;

	// Use engine's built-in cylinder mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(CylinderMesh.Object);
	}

	MeshComp->SetWorldScale3D(FVector(0.3f, 0.3f, 0.15f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AVoltaMotorActor::Initialize(FVoltaShmClient* InShmClient, int32 InPwmChannel)
{
	ShmClient = InShmClient;
	PwmChannel = InPwmChannel;
}

void AVoltaMotorActor::BeginPlay()
{
	Super::BeginPlay();
}

void AVoltaMotorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ShmClient || !ShmClient->IsValid())
	{
		return;
	}

	if (!ShmClient->IsPwmEnabled(PwmChannel))
	{
		return;
	}

	float DutyCycle = ShmClient->ReadPwmDutyCycle(PwmChannel);
	float RPM = DutyCycle * MaxRPM;

	// Accumulate rotation angle: degrees = RPM / 60 * 360 * dt
	CurrentAngle += (RPM / 60.0f) * 360.0f * DeltaTime;

	// Wrap angle to avoid float overflow
	if (CurrentAngle > 360000.0f)
	{
		CurrentAngle -= 360000.0f;
	}

	// Apply rotation around Z axis (cylinder spins around its length)
	FRotator NewRotation = FRotator(0.0f, CurrentAngle, 0.0f);
	SetActorRotation(NewRotation);
}
