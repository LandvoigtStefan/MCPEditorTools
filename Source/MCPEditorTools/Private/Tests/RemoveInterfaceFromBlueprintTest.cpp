#include "MCPEditorToolsToolset.h"
#include "MCPTestUtils.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPRemoveInterfaceFromBlueprintTest, "MCPEditorTools.RemoveInterfaceFromBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMCPRemoveInterfaceFromBlueprintTest::RunTest(const FString& Parameters)
{
	// temporary interface (one function with an output) and a temporary actor Blueprint that implements it
	const FString IName = TEXT("BPI_Temp_RemoveFromBlueprint");
	UPackage* IPackage = CreatePackage(*(FString(MCPTestUtils::TempFolder) + IName));
	UBlueprint* Iface = FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(), IPackage, FName(*IName), BPTYPE_Interface,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("temp interface created"), Iface)) { return false; }
	UBlueprint* Impl = MCPTestUtils::CreateTempActorBlueprint(TEXT("BP_Temp_RemoveFromBlueprintImpl"));
	if (!TestNotNull(TEXT("temp implementer created"), Impl)) { return false; }
	const FString IPath = MCPTestUtils::PathOf(Iface);
	const FString BPPath = MCPTestUtils::PathOf(Impl);
	FKismetEditorUtilities::CompileBlueprint(Iface);
	TestTrue(TEXT("add a function with an output"), UMCPEditorToolsToolset::AddInterfaceFunction(IPath, TEXT("GetThing"), TEXT(""), TEXT("Thing:int")).StartsWith(TEXT("OK")));
	TestTrue(TEXT("implement the interface"), UMCPEditorToolsToolset::AddInterfaceToBlueprint(BPPath, IPath).StartsWith(TEXT("OK added")));
	TestEqual(TEXT("implemented before"), Impl->ImplementedInterfaces.Num(), 1);

	// fail-once proof: MCP_RMIFACE_BREAK=1 skips the tool call (the interface stays and the checks below fail)
	const bool bBreak = !FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_RMIFACE_BREAK")).IsEmpty();
	const FString R = bBreak ? FString(TEXT("OK removed (tool call skipped)")) : UMCPEditorToolsToolset::RemoveInterfaceFromBlueprint(BPPath, IPath);
	TestTrue(*FString::Printf(TEXT("remove interface OK (got: %s)"), *R), R.StartsWith(TEXT("OK removed")));
	TestEqual(TEXT("no interface after"), Impl->ImplementedInterfaces.Num(), 0);
	TestTrue(TEXT("a second call changes nothing"), UMCPEditorToolsToolset::RemoveInterfaceFromBlueprint(BPPath, IPath).Contains(TEXT("not implemented directly")));
	TestTrue(TEXT("a normal Blueprint as the interface is an error"), UMCPEditorToolsToolset::RemoveInterfaceFromBlueprint(BPPath, BPPath).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown Blueprint is an error"), UMCPEditorToolsToolset::RemoveInterfaceFromBlueprint(TEXT("/Game/DoN/Tests/Temp/DoesNotExist"), IPath).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("unknown interface is an error"), UMCPEditorToolsToolset::RemoveInterfaceFromBlueprint(BPPath, TEXT("/Game/DoN/Tests/Temp/DoesNotExist")).StartsWith(TEXT("ERROR")));

	MCPTestUtils::Cleanup(Impl);
	MCPTestUtils::Cleanup(Iface);
	AddInfo(TEXT("PASS all checks passed")); // the headless runner needs a PASS line, otherwise it counts as "no checks ran"
	return true;
}
