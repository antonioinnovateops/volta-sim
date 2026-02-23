#include "VoltaSim.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FVoltaSimModule, VoltaSim, "VoltaSim");

void FVoltaSimModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("VoltaSim module loaded"));
}

void FVoltaSimModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("VoltaSim module unloaded"));
}
