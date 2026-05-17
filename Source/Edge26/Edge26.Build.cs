// Copyright Edge26. All Rights Reserved.

using UnrealBuildTool;

public class Edge26 : ModuleRules
{
	public Edge26(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicIncludePaths.AddRange(new string[]
		{
			"Edge26/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Edge26/Private"
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"PhysicsCore",
			"Chaos",
			"UMG",
			"Slate",
			"SlateCore",
			"Edge26Sim"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"RenderCore",
			"PoseSearch",
			"StructUtils",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("AssetTools");
			// AnimGraph editing for headless AnimBP wiring (UAnimDatabaseUtility).
			PrivateDependencyModuleNames.Add("AnimGraph");
			PrivateDependencyModuleNames.Add("BlueprintGraph");
			PrivateDependencyModuleNames.Add("KismetCompiler");
			PrivateDependencyModuleNames.Add("PoseSearchEditor");
		}
	}
}
