#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPEventReplicationTest, "MCPEditorTools.EventReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPEventReplicationTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_EventReplication"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	MCPTestUtils::AddCustomEvent(BP, TEXT("TestEvent"));
	const FString Path = MCPTestUtils::PathOf(BP);

	TestEqual(TEXT("default is not replicated"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("TestEvent")), FString(TEXT("mode=NotReplicated reliable=false")));

	const FString R1 = UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("TestEvent"), TEXT("Server"), true);
	TestTrue(TEXT("set Server+Reliable returns OK"), R1.StartsWith(TEXT("OK")));
	TestEqual(TEXT("Server reliable"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("TestEvent")), FString(TEXT("mode=Server reliable=true")));

	UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("TestEvent"), TEXT("Multicast"), false);
	TestEqual(TEXT("Multicast unreliable"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("TestEvent")), FString(TEXT("mode=Multicast reliable=false")));

	UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("TestEvent"), TEXT("Client"), true);
	TestEqual(TEXT("Client reliable"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("TestEvent")), FString(TEXT("mode=Client reliable=true")));

	UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("TestEvent"), TEXT("NotReplicated"), true);
	TestEqual(TEXT("back to not replicated"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("TestEvent")), FString(TEXT("mode=NotReplicated reliable=false")));

	TestTrue(TEXT("unknown event is an error"), UMCPEditorToolsToolset::GetEventReplication(Path, TEXT("Nope")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown mode is an error"), UMCPEditorToolsToolset::SetEventReplication(Path, TEXT("TestEvent"), TEXT("Banana"), false).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown blueprint is an error"), UMCPEditorToolsToolset::GetEventReplication(TEXT("/Game/DoN/Tests/Temp/DoesNotExist"), TEXT("X")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(BP);
	return true;
}
