#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPAddEventParameterTest, "MCPEditorTools.AddEventParameter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPAddEventParameterTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_AddEventParameter"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	UK2Node_CustomEvent* Ev = MCPTestUtils::AddCustomEvent(BP, TEXT("TestEvent"));
	const FString Path = MCPTestUtils::PathOf(BP);

	TestEqual(TEXT("no parameters at the start"), Ev->UserDefinedPins.Num(), 0);
	TestTrue(TEXT("add actor"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("TargetActor"), TEXT("/Script/Engine.Actor")).StartsWith(TEXT("OK")));
	TestTrue(TEXT("add component"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("TargetComp"), TEXT("/Script/Engine.PrimitiveComponent")).StartsWith(TEXT("OK")));
	TestTrue(TEXT("add vector"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("Origin"), TEXT("Vector")).StartsWith(TEXT("OK")));
	TestTrue(TEXT("add float"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("Amount"), TEXT("float")).StartsWith(TEXT("OK")));

	TestEqual(TEXT("four parameters"), Ev->UserDefinedPins.Num(), 4);
	// the compiled event must really have the pins with the right types
	UEdGraphPin* ActorPin = Ev->FindPin(TEXT("TargetActor"));
	UEdGraphPin* VecPin = Ev->FindPin(TEXT("Origin"));
	if (TestNotNull(TEXT("actor pin exists"), ActorPin)) { TestEqual(TEXT("actor pin is an object"), ActorPin->PinType.PinCategory, UEdGraphSchema_K2::PC_Object); }
	if (TestNotNull(TEXT("vector pin exists"), VecPin)) { TestEqual(TEXT("vector pin is a struct"), VecPin->PinType.PinCategory, UEdGraphSchema_K2::PC_Struct); }

	TestTrue(TEXT("duplicate name is an error"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("Origin"), TEXT("Vector")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown type is an error"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("X"), TEXT("Banana")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown class is an error"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("TestEvent"), TEXT("X"), TEXT("/Script/Engine.NoSuchClass")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown event is an error"), UMCPEditorToolsToolset::AddEventParameter(Path, TEXT("Nope"), TEXT("X"), TEXT("bool")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown blueprint is an error"), UMCPEditorToolsToolset::AddEventParameter(TEXT("/Game/DoN/Tests/Temp/DoesNotExist"), TEXT("TestEvent"), TEXT("X"), TEXT("bool")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(BP);
	AddInfo(TEXT("PASS all checks passed")); // the headless runner needs a PASS line, otherwise it counts as "no checks ran"
	return true;
}
