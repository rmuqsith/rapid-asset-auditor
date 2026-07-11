// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

using UnrealBuildTool;

public class RPDAssetAuditor : ModuleRules
{
	public RPDAssetAuditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE version defines for cross-version compatibility
		PublicDefinitions.Add("RPD_UE_MAJOR_VERSION=" + Target.Version.MajorVersion.ToString());
		PublicDefinitions.Add("RPD_UE_MINOR_VERSION=" + Target.Version.MinorVersion.ToString());

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AssetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
				"ContentBrowser",
				"InputCore",
				"WorkspaceMenuStructure",
				"Projects",
				"Json",
				"ApplicationCore",
"ToolMenus",
				"Niagara",
			}
		);
	}
}
