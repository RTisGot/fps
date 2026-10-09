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

		// Project Settings に独自の設定ページを出す(Sava Abilities)
		PublicDependencyModuleNames.Add("DeveloperSettings");

		// 部屋(オンラインセッション)の作成・検索・参加。中身(Null / EOS)は DefaultEngine.ini の [OnlineSubsystem] で切り替える
		PublicDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils", "EngineSettings" });
	}
}
