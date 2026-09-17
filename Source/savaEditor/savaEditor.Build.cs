using UnrealBuildTool;
using System.IO;

public class savaEditor : ModuleRules
{
	public savaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "sava", "UnrealEd", "UMG", "UMGEditor", "Slate", "SlateCore", "AssetRegistry", "Kismet", "KismetCompiler", "BlueprintGraph", "MovieScene", "MovieSceneTracks" });
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "../sava"));
	}
}
