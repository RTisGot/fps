// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class sava : ModuleRules
{
	public sava(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 直下のファイル(savaCharacter.h など)を Public/Private 配下からも include できるようにする
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });
		PublicDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Gameplay Ability System(スキル・ガジェット・武器の土台)
		PublicDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTags", "GameplayTasks" });
	}
}
