// Copyright Edge26. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Edge26EditorTarget : TargetRules
{
	public Edge26EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Edge26");
	}
}
