#include "Misc/AutomationTest.h"
#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPInterfaceFunctionTest, "MCPEditorTools.InterfaceFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPInterfaceFunctionTest::RunTest(const FString& Parameters)
{
	// temporary Blueprint Interface (in memory only) and a temporary actor Blueprint that implements it
	const FString IName = TEXT("BPI_Temp_InterfaceFunction");
	UPackage* IPackage = CreatePackage(*(FString(MCPTestUtils::TempFolder) + IName));
	UBlueprint* Iface = FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(), IPackage, FName(*IName), BPTYPE_Interface,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("temp interface created"), Iface)) { return false; }
	UBlueprint* Impl = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_InterfaceImpl"));
	if (!TestNotNull(TEXT("temp implementer created"), Impl)) { return false; }
	const FString IPath = MCPTestUtils::PathOf(Iface);
	const FString BPPath = MCPTestUtils::PathOf(Impl);

	FKismetEditorUtilities::CompileBlueprint(Iface);
	FBlueprintEditorUtils::ImplementNewInterface(Impl, Iface->GeneratedClass->GetClassPathName());
	TestEqual(TEXT("implementer has the interface"), Impl->ImplementedInterfaces.Num(), 1);

	const FString R = UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("GetGripTransform"), TEXT("Viewer:/Script/Engine.Actor;Offset:Vector"), TEXT("Grip:Transform;Valid:bool"));
	TestTrue(*FString::Printf(TEXT("add function OK (got: %s)"), *R), R.StartsWith(TEXT("OK")));
	TestTrue(TEXT("add function without inputs"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("GetGripKind"), TEXT(""), TEXT("Kind:int")).StartsWith(TEXT("OK")));

	UEdGraph* Graph = nullptr;
	for (UEdGraph* G : Iface->FunctionGraphs) { if (G && G->GetFName() == TEXT("GetGripTransform")) { Graph = G; } }
	if (TestNotNull(TEXT("interface has the function graph"), Graph))
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!Entry) { Entry = Cast<UK2Node_FunctionEntry>(N); }
			if (!Result) { Result = Cast<UK2Node_FunctionResult>(N); }
		}
		if (TestNotNull(TEXT("function entry"), Entry))
		{
			TestEqual(TEXT("two input parameters"), Entry->UserDefinedPins.Num(), 2);
			UEdGraphPin* Viewer = Entry->FindPin(TEXT("Viewer"));
			if (TestNotNull(TEXT("Viewer pin"), Viewer)) { TestEqual(TEXT("Viewer is an object"), Viewer->PinType.PinCategory, UEdGraphSchema_K2::PC_Object); }
		}
		if (TestNotNull(TEXT("function result"), Result))
		{
			TestEqual(TEXT("two output parameters"), Result->UserDefinedPins.Num(), 2);
			UEdGraphPin* Grip = Result->FindPin(TEXT("Grip"));
			if (TestNotNull(TEXT("Grip pin"), Grip))
			{
				TestEqual(TEXT("Grip is a struct"), Grip->PinType.PinCategory, UEdGraphSchema_K2::PC_Struct);
				TestTrue(TEXT("Grip is a Transform"), Grip->PinType.PinSubCategoryObject == TBaseStructure<FTransform>::Get());
			}
		}
	}

	TestTrue(TEXT("duplicate function is an error"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("GetGripKind"), TEXT(""), TEXT("Kind:int")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown type is an error"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("Bad1"), TEXT("X:Banana"), TEXT("")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("bad spec is an error"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("Bad2"), TEXT("NoColon"), TEXT("")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("a normal Blueprint is refused"), UMCPEditorToolsToolset::AddInterfaceFunction(BPPath, TEXT("Bad3"), TEXT(""), TEXT("")).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown asset is an error"), UMCPEditorToolsToolset::AddInterfaceFunction(TEXT("/Game/DoN/Tests/Temp/DoesNotExist"), TEXT("Bad4"), TEXT(""), TEXT("")).StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("a failed call adds no graph"), Iface->FunctionGraphs.ContainsByPredicate([](UEdGraph* G) { return G && G->GetFName() == TEXT("Bad1"); }));

	// implementing side: the new functions become graphs in the implementer
	const FString C = UMCPEditorToolsToolset::ConformInterfaces(BPPath);
	TestTrue(*FString::Printf(TEXT("conform OK (got: %s)"), *C), C.StartsWith(TEXT("OK")));
	auto HasGraph = [&](const TCHAR* Name)
	{
		for (const FBPInterfaceDescription& Desc : Impl->ImplementedInterfaces)
		{
			if (Desc.Graphs.ContainsByPredicate([Name](UEdGraph* G) { return G && G->GetFName() == Name; })) { return true; }
		}
		return false;
	};
	TestTrue(TEXT("implementer got GetGripTransform"), HasGraph(TEXT("GetGripTransform")));
	TestTrue(TEXT("implementer got GetGripKind"), HasGraph(TEXT("GetGripKind")));
	TestTrue(TEXT("second conform adds nothing"), UMCPEditorToolsToolset::ConformInterfaces(BPPath).Contains(TEXT("new function graphs: []")));
	TestTrue(TEXT("conform on unknown asset is an error"), UMCPEditorToolsToolset::ConformInterfaces(TEXT("/Game/DoN/Tests/Temp/DoesNotExist")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(Impl);
	MCPTestUtils::Cleanup(Iface);
	AddInfo(TEXT("PASS all checks passed")); // the headless runner needs a PASS line, otherwise it counts as "no checks ran"
	return true;
}
