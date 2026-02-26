#include "VoltaGameMode.h"
#include "Board/VoltaBoardActor.h"

AVoltaGameMode::AVoltaGameMode()
{
}

void AVoltaGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogTemp, Log, TEXT("VoltaGameMode: Spawning VoltaBoardActor at origin"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GetWorld()->SpawnActor<AVoltaBoardActor>(
		AVoltaBoardActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);
}
