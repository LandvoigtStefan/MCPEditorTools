using UnrealBuildTool;

public class MCPEditorTools : ModuleRules
{
	public MCPEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ToolsetRegistry",
			"UnrealEd",
			"AssetRegistry",
			"Kismet",
			"KismetCompiler",
			"BlueprintGraph",
			"GraphEditor",
		});
	}
}
