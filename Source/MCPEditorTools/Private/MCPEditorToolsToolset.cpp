#include "MCPEditorToolsToolset.h"
#include "MCPToolUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"

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

FString UMCPEditorToolsToolset::ReloadOrDiscardAsset(const TArray<FString>& AssetPaths, bool bDryRun)
{
	TArray<FString> Lines;
	TArray<UPackage*> ToReload;
	int32 DirtyCount = 0;
	for (const FString& Path : AssetPaths)
	{
		UPackage* Package = FindPackage(nullptr, *Path);
		if (!Package) { Lines.Add(Path + TEXT(" | not loaded, skipped")); continue; }
		if (!Package->IsDirty()) { Lines.Add(Path + TEXT(" | clean, skipped")); continue; }
		if (!FPackageName::DoesPackageExist(Path)) { Lines.Add(Path + TEXT(" | dirty but NOT on disk (new asset), skipped")); continue; }
		++DirtyCount;
		ToReload.Add(Package);
		Lines.Add(Path + (bDryRun ? TEXT(" | dirty, would reload") : TEXT(" | dirty, reloading")));
	}

	int32 Reloaded = 0;
	if (!bDryRun && ToReload.Num() > 0)
	{
		FText ErrorMessage;
		const bool bOk = UPackageTools::ReloadPackages(ToReload, ErrorMessage, EReloadPackagesInteractionMode::AssumePositive);
		if (!bOk) { Lines.Add(TEXT("reload reported a problem: ") + ErrorMessage.ToString()); }
		for (const FString& Path : AssetPaths)
		{
			UPackage* Package = FindPackage(nullptr, *Path);
			// After a reload the old package may be replaced; the asset counts as reloaded when it is loaded again and clean.
			if (!Package) { Package = LoadPackage(nullptr, *Path, LOAD_None); }
			if (Package && !Package->IsDirty() && ToReload.ContainsByPredicate([&](UPackage* P) { return P && P->GetName() == Path; })) { ++Reloaded; }
			else if (Package && Package->IsDirty()) { Lines.Add(Path + TEXT(" | STILL DIRTY after reload")); }
		}
	}
	return FString::Printf(TEXT("dry_run=%s dirty=%d reloaded=%d\n%s"), bDryRun ? TEXT("true") : TEXT("false"), DirtyCount, Reloaded, *FString::Join(Lines, TEXT("\n")));
}

FString UMCPEditorToolsToolset::ListCustomEvents(const FString& Path)
{
	TArray<FString> BlueprintPaths;
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	ARM.Get().GetAssetsByPath(FName(*Path), Assets, true);
	for (const FAssetData& A : Assets)
	{
		if (A.IsInstanceOf(UBlueprint::StaticClass())) { BlueprintPaths.Add(A.PackageName.ToString()); }
	}
	if (BlueprintPaths.Num() == 0) { BlueprintPaths.Add(Path); }

	TArray<FString> Lines;
	for (const FString& BPPath : BlueprintPaths)
	{
		FString Err;
		UBlueprint* BP = MCPToolUtils::LoadBlueprint(BPPath, Err);
		if (!BP)
		{
			if (BlueprintPaths.Num() == 1) { return Err; }
			continue;
		}
		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_CustomEvent* Ev = Cast<UK2Node_CustomEvent>(Node))
				{
					Lines.Add(FString::Printf(TEXT("%s | %s | %s"), *BPPath, *Ev->CustomFunctionName.ToString(), *DescribeReplication(Ev->FunctionFlags)));
				}
			}
		}
	}
	return FString::Printf(TEXT("blueprints=%d events=%d\n%s"), BlueprintPaths.Num(), Lines.Num(), *FString::Join(Lines, TEXT("\n")));
}

FString UMCPEditorToolsToolset::FindUnconnectedNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	TArray<UEdGraph*> Graphs;
	if (GraphName == TEXT("*"))
	{
		BP->GetAllGraphs(Graphs);
	}
	else if (UEdGraph* Found = MCPToolUtils::FindGraph(BP, GraphName))
	{
		Graphs.Add(Found);
	}
	else
	{
		return MCPToolUtils::Error(FString::Printf(TEXT("graph '%s' not found in %s"), *GraphName, *BlueprintPath));
	}

	TArray<UEdGraphNode*> ToRemove;
	TArray<FString> Lines;
	for (UEdGraph* Graph : Graphs)
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
		Lines.Add(FString::Printf(TEXT("%s | %s | %s | %s | %d,%d%s"), *Graph->GetName(), *Node->GetName(), *Node->GetClass()->GetName(),
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

FString UMCPEditorToolsToolset::FindDeadNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	TArray<UEdGraph*> Graphs;
	if (GraphName == TEXT("*")) { BP->GetAllGraphs(Graphs); }
	else if (UEdGraph* Found = MCPToolUtils::FindGraph(BP, GraphName)) { Graphs.Add(Found); }
	else { return MCPToolUtils::Error(FString::Printf(TEXT("graph '%s' not found in %s"), *GraphName, *BlueprintPath)); }

	TArray<FString> Lines;
	TArray<TPair<UEdGraph*, UEdGraphNode*>> ToRemove;
	for (UEdGraph* Graph : Graphs)
	{
		TSet<UEdGraphNode*> Live;
		TArray<UEdGraphNode*> Stack;
		auto MarkLive = [&](UEdGraphNode* N) { if (N && !Live.Contains(N)) { Live.Add(N); Stack.Add(N); } };
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UK2Node_Event>(Node) || Cast<UK2Node_FunctionEntry>(Node) || Cast<UK2Node_FunctionResult>(Node)) { MarkLive(Node); }
		}
		while (Stack.Num() > 0)
		{
			UEdGraphNode* N = Stack.Pop();
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin) { continue; }
				const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
				if (bExec && Pin->Direction == EGPD_Output)
				{
					for (UEdGraphPin* L : Pin->LinkedTo) { MarkLive(L->GetOwningNode()); } // execution flow forward
				}
				else if (!bExec && Pin->Direction == EGPD_Input)
				{
					for (UEdGraphPin* L : Pin->LinkedTo) { MarkLive(L->GetOwningNode()); } // data sources of a live node
				}
			}
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || Live.Contains(Node) || Cast<UEdGraphNode_Comment>(Node)) { continue; }
			Lines.Add(FString::Printf(TEXT("%s | %s | %s | %s | %d,%d"), *Graph->GetName(), *Node->GetName(), *Node->GetClass()->GetName(),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" ")), Node->NodePosX, Node->NodePosY));
			ToRemove.Add(TPair<UEdGraph*, UEdGraphNode*>(Graph, Node));
		}
	}

	int32 Removed = 0;
	if (!bDryRun && ToRemove.Num() > 0)
	{
		for (const TPair<UEdGraph*, UEdGraphNode*>& Pair : ToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(BP, Pair.Value, true);
			++Removed;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
	}
	return FString::Printf(TEXT("dry_run=%s dead=%d removed=%d\n%s"), bDryRun ? TEXT("true") : TEXT("false"), ToRemove.Num(), Removed, *FString::Join(Lines, TEXT("\n")));
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
