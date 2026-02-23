using UnrealBuildTool;

public class VoltaSim : ModuleRules
{
	public VoltaSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"Slate",
			"SlateCore"
		});

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("VOLTA_PLATFORM_LINUX=1");
		}
	}
}
