#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "EdGraphSchema_K2.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPFindDeadNodesTest, "MCPEditorTools.FindDeadNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static UK2Node_CallFunction* AddCall(UEdGraph* Graph, UClass* Lib, FName Func)
{
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->SetFromFunction(Lib->FindFunctionByName(Func));
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	return Node;
}

bool FMCPFindDeadNodesTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_FindDead"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
	const FString Path = MCPTestUtils::PathOf(BP);
	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	auto Link = [&](UEdGraphNode* A, FName PinA, UEdGraphNode* B, FName PinB) { return K2->TryCreateConnection(A->FindPinChecked(PinA), B->FindPinChecked(PinB)); };

	// LIVE: Ev -> Print1, Print1.InString <- Concat1 (pure data source)
	UK2Node_CustomEvent* Ev = MCPTestUtils::AddCustomEvent(BP, TEXT("Ev"));
	UK2Node_CallFunction* Print1 = AddCall(Graph, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	UK2Node_CallFunction* Concat1 = AddCall(Graph, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
	TestTrue(TEXT("link ev->print1"), Link(Ev, UEdGraphSchema_K2::PN_Then, Print1, UEdGraphSchema_K2::PN_Execute));
	TestTrue(TEXT("link concat1->print1"), Link(Concat1, UEdGraphSchema_K2::PN_ReturnValue, Print1, TEXT("InString")));

	// DEAD 1+2: Print2 -> Print3 (a cluster connected to each other, no event)
	UK2Node_CallFunction* Print2 = AddCall(Graph, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	UK2Node_CallFunction* Print3 = AddCall(Graph, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	TestTrue(TEXT("link print2->print3"), Link(Print2, UEdGraphSchema_K2::PN_Then, Print3, UEdGraphSchema_K2::PN_Execute));
	// DEAD 3: Concat2 feeds only dead Print3
	UK2Node_CallFunction* Concat2 = AddCall(Graph, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
	TestTrue(TEXT("link concat2->print3"), Link(Concat2, UEdGraphSchema_K2::PN_ReturnValue, Print3, TEXT("InString")));
	// DEAD 4: unused pure node fed by a LIVE node (like a leftover "bind"): input linked, output unused
	UK2Node_CallFunction* Concat3 = AddCall(Graph, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
	TestTrue(TEXT("link concat1->concat3"), Link(Concat1, UEdGraphSchema_K2::PN_ReturnValue, Concat3, TEXT("A")));
	const int32 NodesBefore = Graph->Nodes.Num();

	const FString Dry = UMCPEditorToolsToolset::FindDeadNodes(Path, Graph->GetName(), true);
	TestTrue(TEXT("dry run finds 4 dead"), Dry.StartsWith(TEXT("dry_run=true dead=4 removed=0")));
	TestTrue(TEXT("Print2 dead"), Dry.Contains(Print2->GetName()));
	TestTrue(TEXT("Print3 dead"), Dry.Contains(Print3->GetName()));
	TestTrue(TEXT("Concat2 dead"), Dry.Contains(Concat2->GetName()));
	TestTrue(TEXT("Concat3 dead (unused pure node with a linked input)"), Dry.Contains(Concat3->GetName()));
	TestFalse(TEXT("Print1 live"), Dry.Contains(Print1->GetName()));
	TestFalse(TEXT("Concat1 live"), Dry.Contains(Concat1->GetName()));
	TestFalse(TEXT("event live"), Dry.Contains(Ev->GetName()));
	TestEqual(TEXT("dry run removed nothing"), Graph->Nodes.Num(), NodesBefore);

	const FString Real = UMCPEditorToolsToolset::FindDeadNodes(Path, TEXT("*"), false);
	TestTrue(TEXT("real run removed 4"), Real.StartsWith(TEXT("dry_run=false dead=4 removed=4")));
	TestEqual(TEXT("only live nodes remain (Ev, Print1, Concat1)"), Graph->Nodes.Num(), NodesBefore - 4);
	TestTrue(TEXT("second run finds nothing"), UMCPEditorToolsToolset::FindDeadNodes(Path, TEXT("*"), true).StartsWith(TEXT("dry_run=true dead=0")));
	TestTrue(TEXT("unknown graph is an error"), UMCPEditorToolsToolset::FindDeadNodes(Path, TEXT("NoSuchGraph"), true).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(BP);
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}


