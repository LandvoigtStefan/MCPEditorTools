#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPListCustomEventsTest, "MCPEditorTools.ListCustomEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPListCustomEventsTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_ListEvents"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	MCPTestUtils::AddCustomEvent(BP, TEXT("EvA"));
	MCPTestUtils::AddCustomEvent(BP, TEXT("EvB"));
	const FString Path = MCPTestUtils::PathOf(BP);
	UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("EvB"), TEXT("Server"), true);

	const FString R = UMCPEditorToolsToolset::ListCustomEvents(Path);
	TestTrue(TEXT("header says 1 blueprint, 2 events"), R.StartsWith(TEXT("blueprints=1 events=2")));
	TestTrue(TEXT("lists EvA not replicated"), R.Contains(Path + TEXT(" | EvA | mode=NotReplicated reliable=false")));
	TestTrue(TEXT("lists EvB server reliable"), R.Contains(Path + TEXT(" | EvB | mode=Server reliable=true")));
	TestTrue(TEXT("unknown path is an error"), UMCPEditorToolsToolset::ListCustomEvents(TEXT("/Game/DoN/Tests/Temp/NoSuch")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(BP);
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}

