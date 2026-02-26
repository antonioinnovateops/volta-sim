#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VoltaGameMode.generated.h"

/**
 * Default GameMode for Volta-Sim.
 * Spawns VoltaBoardActor at world origin on BeginPlay,
 * so ANY empty level works as the default map.
 */
UCLASS()
class VOLTASIM_API AVoltaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVoltaGameMode();

	virtual void StartPlay() override;
};
