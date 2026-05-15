// Copyright Edge26. All Rights Reserved.

using UnrealBuildTool;

public class Edge26Sim : ModuleRules
{
	public Edge26Sim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicIncludePaths.AddRange(new string[]
		{
			"Edge26Sim/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Edge26Sim/Private"
		});

		// Determinism boundary: Core only. NO Engine, Chaos, AnimGraphRuntime, etc.
		// Adding anything else requires a doc PR. See spec §4.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		bUseUnity = false;
	}
}
