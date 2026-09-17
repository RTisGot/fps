// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class sava : ModuleRules
{
	public sava(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });
		PublicDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
