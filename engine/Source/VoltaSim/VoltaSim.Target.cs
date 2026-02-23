using UnrealBuildTool;

public class VoltaSimTarget : TargetRules
{
	public VoltaSimTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("VoltaSim");
	}
}
