// Copyright Edge26. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Edge26Target : TargetRules
{
	public Edge26Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Edge26");
	}
}
