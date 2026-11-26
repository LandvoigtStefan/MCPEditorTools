#include "MCPEditorToolsToolset.h"
#include "MCPToolUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"

FString UMCPEditorToolsToolset::Ping()
{
	return TEXT("pong (MCPEditorTools 0.1)");
}

static FString DescribeReplication(uint32 Flags)
{
	const TCHAR* Mode = TEXT("NotReplicated");
	if (Flags & FUNC_NetServer) { Mode = TEXT("Server"); }
	else if (Flags & FUNC_NetClient) { Mode = TEXT("Client"); }
	else if (Flags & FUNC_NetMulticast) { Mode = TEXT("Multicast"); }
	const bool bReliable = (Flags & FUNC_NetReliable) != 0;
	return FString::Printf(TEXT("mode=%s reliable=%s"), Mode, bReliable ? TEXT("true") : TEXT("false"));
}

FString UMCPEditorToolsToolset::GetEventReplication(const FString& BlueprintPath, const FString& EventName)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UK2Node_CustomEvent* Ev = MCPToolUtils::FindCustomEvent(BP, EventName);
	if (!Ev) { return MCPToolUtils::Error(FString::Printf(TEXT("custom event '%s' not found in %s"), *EventName, *BlueprintPath)); }
	return DescribeReplication(Ev->FunctionFlags);
}

FString UMCPEditorToolsToolset::FindUnconnectedNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UEdGraph* Graph = MCPToolUtils::FindGraph(BP, GraphName);
	if (!Graph) { return MCPToolUtils::Error(FString::Printf(TEXT("graph '%s' not found in %s"), *GraphName, *BlueprintPath)); }

	TArray<UEdGraphNode*> ToRemove;
	TArray<FString> Lines;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || Cast<UEdGraphNode_Comment>(Node) || Cast<UK2Node_FunctionEntry>(Node) || Cast<UK2Node_FunctionResult>(Node)) { continue; }
		bool bAnyLink = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->LinkedTo.Num() > 0) { bAnyLink = true; break; }
		}
		if (bAnyLink) { continue; }
		const bool bIsEvent = Cast<UK2Node_Event>(Node) != nullptr;
		Lines.Add(FString::Printf(TEXT("%s | %s | %s | %d,%d%s"), *Node->GetName(), *Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" ")), Node->NodePosX, Node->NodePosY,
			bIsEvent ? TEXT(" [event, kept]") : TEXT("")));
		if (!bIsEvent) { ToRemove.Add(Node); }
	}

	int32 Removed = 0;
	if (!bDryRun && ToRemove.Num() > 0)
	{
		for (UEdGraphNode* Node : ToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(BP, Node, true);
			++Removed;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
	}
	return FString::Printf(TEXT("dry_run=%s candidates=%d removed=%d\n%s"), bDryRun ? TEXT("true") : TEXT("false"), ToRemove.Num(), Removed, *FString::Join(Lines, TEXT("\n")));
}

FString UMCPEditorToolsToolset::SetEventReplication(const FString& BlueprintPath, const FString& EventName, const FString& Mode, bool bReliable)
{
	uint32 NewFlags = 0;
	if (Mode.Equals(TEXT("Server"), ESearchCase::IgnoreCase)) { NewFlags = FUNC_Net | FUNC_NetServer; }
	else if (Mode.Equals(TEXT("Client"), ESearchCase::IgnoreCase)) { NewFlags = FUNC_Net | FUNC_NetClient; }
	else if (Mode.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase)) { NewFlags = FUNC_Net | FUNC_NetMulticast; }
	else if (!Mode.Equals(TEXT("NotReplicated"), ESearchCase::IgnoreCase))
	{
		return MCPToolUtils::Error(FString::Printf(TEXT("unknown mode '%s' (use NotReplicated, Server, Client or Multicast)"), *Mode));
	}
	if (NewFlags != 0 && bReliable) { NewFlags |= FUNC_NetReliable; }

	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UK2Node_CustomEvent* Ev = MCPToolUtils::FindCustomEvent(BP, EventName);
	if (!Ev) { return MCPToolUtils::Error(FString::Printf(TEXT("custom event '%s' not found in %s"), *EventName, *BlueprintPath)); }

	const uint32 NetMask = FUNC_Net | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast | FUNC_NetReliable | FUNC_NetRequest | FUNC_NetResponse | FUNC_NetValidate;
	Ev->Modify();
	Ev->FunctionFlags = (Ev->FunctionFlags & ~NetMask) | NewFlags;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return FString::Printf(TEXT("OK %s (asset not saved)"), *DescribeReplication(Ev->FunctionFlags));
}
