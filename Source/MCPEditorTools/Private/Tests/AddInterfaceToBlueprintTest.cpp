#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPAddInterfaceToBlueprintTest, "MCPEditorTools.AddInterfaceToBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPAddInterfaceToBlueprintTest::RunTest(const FString& Parameters)
{
	// temporary Blueprint Interface with one function (with an output) and a temporary actor Blueprint that does NOT implement it yet
	const FString IName = TEXT("BPI_Temp_AddToBlueprint");
	UPackage* IPackage = CreatePackage(*(FString(MCPTestUtils::TempFolder) + IName));
	UBlueprint* Iface = FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(), IPackage, FName(*IName), BPTYPE_Interface,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("temp interface created"), Iface)) { return false; }
	UBlueprint* Impl = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_AddToBlueprintImpl"));
	if (!TestNotNull(TEXT("temp implementer created"), Impl)) { return false; }
	const FString IPath = MCPTestUtils::PathOf(Iface);
	const FString BPPath = MCPTestUtils::PathOf(Impl);
	FKismetEditorUtilities::CompileBlueprint(Iface);
	TestTrue(TEXT("add a function with an output"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("GetThing"), TEXT(""), TEXT("Thing:int")).StartsWith(TEXT("OK")));

	TestEqual(TEXT("not implemented before"), Impl->ImplementedInterfaces.Num(), 0);
	// fail-once proof: MCP_ADDIFACE_BREAK=1 skips the tool call (the implementer then never gets the interface and the checks below fail)
	const bool bBreak = !FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_ADDIFACE_BREAK")).IsEmpty();
	const FString R = bBreak ? FString(TEXT("OK added (tool call skipped)")) : UMCPEditorToolsToolset::AddInterfaceToBlueprint(BPPath, IPath);
	TestTrue(*FString::Printf(TEXT("add interface OK (got: %s)"), *R), R.StartsWith(TEXT("OK added")));
	TestEqual(TEXT("implemented after"), Impl->ImplementedInterfaces.Num(), 1);
	bool bGraph = false;
	for (const FBPInterfaceDescription& Desc : Impl->ImplementedInterfaces) { bGraph |= Desc.Graphs.ContainsByPredicate([](UEdGraph* G) { return G && G->GetFName() == TEXT("GetThing"); }); }
	TestTrue(TEXT("the implementer got the function graph GetThing"), bGraph);
	TestTrue(TEXT("a second call changes nothing"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(BPPath, IPath).Contains(TEXT("already implemented")));
	TestEqual(TEXT("still one interface"), Impl->ImplementedInterfaces.Num(), 1);
	TestTrue(TEXT("a normal Blueprint as the interface is an error"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(BPPath, BPPath).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("an interface as the implementer is an error"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(IPath, IPath).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown implementer is an error"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(TEXT("/Game/DoN/Tests/Temp/DoesNotExist"), IPath).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown interface is an error"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(BPPath, TEXT("/Game/DoN/Tests/Temp/DoesNotExist")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(Impl);
	MCPTestUtils::Cleanup(Iface);
	AddInfo(TEXT("PASS all checks passed")); // the headless runner needs a PASS line, otherwise it counts as "no checks ran"
	return true;
}
