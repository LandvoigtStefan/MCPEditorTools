#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "MCPToolUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPReloadOrDiscardAssetTest, "MCPEditorTools.ReloadOrDiscardAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPReloadOrDiscardAssetTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_ReloadDiscard"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	const FString Path = MCPTestUtils::PathOf(BP);
	const FString File = MCPTestUtils::SaveTempToDisk(BP);
	TestTrue(TEXT("temp asset saved to disk"), IFileManager::Get().FileExists(*File));
	TestFalse(TEXT("clean after save"), BP->GetPackage()->IsDirty());

	// Unsaved edit: add an event and mark dirty.
	MCPTestUtils::AddCustomEvent(BP, TEXT("UnsavedEvent"));
	BP->GetPackage()->MarkPackageDirty();
	TestTrue(TEXT("dirty after edit"), BP->GetPackage()->IsDirty());

	const FString Dry = UMCPEditorToolsToolset::ReloadOrDiscardAsset({ Path }, true);
	TestTrue(TEXT("dry run reports 1 dirty, 0 reloaded"), Dry.StartsWith(TEXT("dry_run=true dirty=1 reloaded=0")));
	TestTrue(TEXT("dry run changed nothing (still dirty, event still there)"), BP->GetPackage()->IsDirty() && MCPToolUtils::FindCustomEvent(BP, TEXT("UnsavedEvent")) != nullptr);

	const FString Real = UMCPEditorToolsToolset::ReloadOrDiscardAsset({ Path }, false);
	TestTrue(TEXT("real run reloaded 1"), Real.StartsWith(TEXT("dry_run=false dirty=1 reloaded=1")));
	FString Err;
	UBlueprint* Reloaded = MCPToolUtils::LoadBlueprint(Path, Err);
	if (TestNotNull(TEXT("blueprint loadable after reload"), Reloaded))
	{
		TestTrue(TEXT("unsaved event is gone after reload"), MCPToolUtils::FindCustomEvent(Reloaded, TEXT("UnsavedEvent")) != nullptr);
		TestFalse(TEXT("clean after reload"), Reloaded->GetPackage()->IsDirty());
	}

	const FString Again = UMCPEditorToolsToolset::ReloadOrDiscardAsset({ Path, TEXT("/Game/DoN/Tests/Temp/NotLoaded") }, true);
	TestTrue(TEXT("clean and unloaded assets are skipped"), Again.StartsWith(TEXT("dry_run=true dirty=0")) && Again.Contains(TEXT("clean, skipped")) && Again.Contains(TEXT("not loaded, skipped")));

	MCPTestUtils::Cleanup(Reloaded ? Reloaded : BP);
	MCPTestUtils::DeleteTempFile(File);
	TestFalse(TEXT("temp file deleted"), IFileManager::Get().FileExists(*File));
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}

