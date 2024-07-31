// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PaperUnreal : ModuleRules
{
	public PaperUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "GeometryCore", "GeometryFramework", "GeometryScriptingCore", "UMG" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput" });
    }
}
