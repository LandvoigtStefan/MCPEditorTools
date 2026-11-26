#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPFindUnconnectedNodesTest, "MCPEditorTools.FindUnconnectedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static UK2Node_CallFunction* AddPrintString(UEdGraph* Graph)
{
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)));
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	return Node;
}

bool FMCPFindUnconnectedNodesTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_FindUnconnected"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
	const FString Path = MCPTestUtils::PathOf(BP);

	UK2Node_CustomEvent* Ev = MCPTestUtils::AddCustomEvent(BP, TEXT("Ev"));
	MCPTestUtils::AddCustomEvent(BP, TEXT("Lonely"));
	UK2Node_CallFunction* Connected = AddPrintString(Graph);
	UK2Node_CallFunction* Loose = AddPrintString(Graph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->TryCreateConnection(Ev->FindPinChecked(UEdGraphSchema_K2::PN_Then), Connected->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
	const int32 NodesBefore = Graph->Nodes.Num();

	const FString Dry = UMCPEditorToolsToolset::FindUnconnectedNodes(Path, Graph->GetName(), true);
	TestTrue(TEXT("dry run reports 1 candidate"), Dry.StartsWith(TEXT("dry_run=true candidates=1 removed=0")));
	TestTrue(TEXT("dry run lists the loose node"), Dry.Contains(Loose->GetName()));
	TestFalse(TEXT("dry run does not list the connected node"), Dry.Contains(Connected->GetName()));
	TestTrue(TEXT("dry run lists the lonely event as kept"), Dry.Contains(TEXT("[event, kept]")));
	TestEqual(TEXT("dry run removed nothing"), Graph->Nodes.Num(), NodesBefore);

	const FString Real = UMCPEditorToolsToolset::FindUnconnectedNodes(Path, Graph->GetName(), false);
	TestTrue(TEXT("real run removed 1"), Real.StartsWith(TEXT("dry_run=false candidates=1 removed=1")));
	TestEqual(TEXT("exactly one node gone"), Graph->Nodes.Num(), NodesBefore - 1);
	TestTrue(TEXT("connected node still there"), Graph->Nodes.Contains(Connected));
	TestTrue(TEXT("lonely event still there"), MCPToolUtils::FindCustomEvent(BP, TEXT("Lonely")) != nullptr);

	const FString All = UMCPEditorToolsToolset::FindUnconnectedNodes(Path, TEXT("*"), true);
	TestTrue(TEXT("'*' scans all graphs (0 removable left, lonely event listed with graph name)"), All.StartsWith(TEXT("dry_run=true candidates=0 removed=0")) && All.Contains(Graph->GetName() + TEXT(" | ")));
	TestTrue(TEXT("unknown graph is an error"), UMCPEditorToolsToolset::FindUnconnectedNodes(Path, TEXT("NoSuchGraph"), true).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(BP);
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}



