#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "EdGraphSchema_K2.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPConnectPinsTest, "MCPEditorTools.ConnectPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPSetFunctionPureTest, "MCPEditorTools.SetFunctionPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static UK2Node_CallFunction* AddCallNode(UEdGraph* Graph, UClass* Lib, FName Func)
{
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->SetFromFunction(Lib->FindFunctionByName(Func));
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	return Node;
}

bool FMCPConnectPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_ConnectPins"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
	const FString Path = MCPTestUtils::PathOf(BP);
	const FString G = Graph->GetName();

	UK2Node_CustomEvent* Ev = MCPTestUtils::AddCustomEvent(BP, TEXT("Ev"));
	UK2Node_CallFunction* Print = AddCallNode(Graph, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	UK2Node_CallFunction* Concat = AddCallNode(Graph, UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));

	// failures must be loud and helpful
	const FString BadPin = UMCPEditorToolsToolset::ConnectPins(Path, G, Ev->GetName(), TEXT("NoSuchPin"), Print->GetName(), TEXT("execute"));
	TestTrue(TEXT("unknown output pin is an error"), BadPin.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("error lists the real output pins"), BadPin.Contains(TEXT("then")));
	TestTrue(TEXT("unknown node is an error"), UMCPEditorToolsToolset::ConnectPins(Path, G, TEXT("NoSuchNode"), TEXT("then"), Print->GetName(), TEXT("execute")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown graph is an error"), UMCPEditorToolsToolset::ConnectPins(Path, TEXT("NoGraph"), Ev->GetName(), TEXT("then"), Print->GetName(), TEXT("execute")).StartsWith(TEXT("ERROR")));
	const FString Refused = UMCPEditorToolsToolset::ConnectPins(Path, G, Ev->GetName(), TEXT("then"), Concat->GetName(), TEXT("A"));
	TestTrue(TEXT("exec -> string pin is refused by the schema"), Refused.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("nothing was linked by the failed calls"), Ev->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(), 0);

	// success: exec link and data link
	const FString Ok1 = UMCPEditorToolsToolset::ConnectPins(Path, G, Ev->GetName(), TEXT("then"), Print->GetName(), TEXT("execute"));
	TestTrue(TEXT("exec link reported OK"), Ok1.StartsWith(TEXT("OK")));
	TestEqual(TEXT("event then is linked once"), Ev->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(), 1);
	TestTrue(TEXT("event then links to Print.execute"), Ev->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo[0]->GetOwningNode() == Print);
	const FString Ok2 = UMCPEditorToolsToolset::ConnectPins(Path, G, Concat->GetName(), TEXT("ReturnValue"), Print->GetName(), TEXT("InString"));
	TestTrue(TEXT("data link reported OK"), Ok2.StartsWith(TEXT("OK")));
	TestEqual(TEXT("InString is linked"), Print->FindPinChecked(TEXT("InString"))->LinkedTo.Num(), 1);

	MCPTestUtils::Cleanup(BP);
	if (HasAnyErrors()) { return false; }
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}

bool FMCPSetFunctionPureTest::RunTest(const FString& Parameters)
{
	UBlueprint* BP = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_SetPure"));
	if (!TestNotNull(TEXT("temp blueprint created"), BP)) { return false; }
	const FString Path = MCPTestUtils::PathOf(BP);

	UEdGraph* Fn = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("TempFn"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, Fn, true, nullptr);
	UK2Node_FunctionEntry* Entry = nullptr;
	for (UEdGraphNode* N : Fn->Nodes) { Entry = Cast<UK2Node_FunctionEntry>(N); if (Entry) { break; } }
	if (!TestNotNull(TEXT("function entry exists"), Entry)) { return false; }
	TestFalse(TEXT("starts impure"), Entry->HasAnyExtraFlags(FUNC_BlueprintPure));

	TestTrue(TEXT("unknown function is an error"), UMCPEditorToolsToolset::SetFunctionPure(Path, TEXT("NoFn"), true).StartsWith(TEXT("ERROR")));

	// refused while an exec node hangs on the entry
	UK2Node_CallFunction* Print = AddCallNode(Fn, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then), Print->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
	TestTrue(TEXT("refused with exec flow"), UMCPEditorToolsToolset::SetFunctionPure(Path, TEXT("TempFn"), true).StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("still impure after refusal"), Entry->HasAnyExtraFlags(FUNC_BlueprintPure));

	// remove the exec node, then pure works and survives the compile
	Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
	Fn->RemoveNode(Print);
	const FString Ok = UMCPEditorToolsToolset::SetFunctionPure(Path, TEXT("TempFn"), true);
	TestTrue(TEXT("set pure reports OK"), Ok.StartsWith(TEXT("OK pure=true")));
	TestTrue(TEXT("entry has the Pure flag"), Entry->HasAnyExtraFlags(FUNC_BlueprintPure));
	UFunction* Compiled = BP->GeneratedClass ? BP->GeneratedClass->FindFunctionByName(TEXT("TempFn")) : nullptr;
	if (TestNotNull(TEXT("compiled function exists"), Compiled))
	{
		TestTrue(TEXT("compiled function is pure"), Compiled->HasAnyFunctionFlags(FUNC_BlueprintPure));
	}

	// and back
	TestTrue(TEXT("set impure reports OK"), UMCPEditorToolsToolset::SetFunctionPure(Path, TEXT("TempFn"), false).StartsWith(TEXT("OK pure=false")));
	TestFalse(TEXT("entry no longer pure"), Entry->HasAnyExtraFlags(FUNC_BlueprintPure));

	MCPTestUtils::Cleanup(BP);
	if (HasAnyErrors()) { return false; }
	AddInfo(TEXT("PASS all checks passed"));
	return true;
}
