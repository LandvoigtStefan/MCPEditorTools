#include "MCPEditorToolsToolset.h"
#include "MCPToolUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "EdGraphSchema_K2.h"
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
	TArray<FString> DirtyPaths;
	for (const FString& Path : AssetPaths)
	{
		UPackage* Package = FindPackage(nullptr, *Path);
		if (!Package) { Lines.Add(Path + TEXT(" | not loaded, skipped")); continue; }
		if (!Package->IsDirty()) { Lines.Add(Path + TEXT(" | clean, skipped")); continue; }
		if (!FPackageName::DoesPackageExist(Path)) { Lines.Add(Path + TEXT(" | dirty but NOT on disk (new asset), skipped")); continue; }
		++DirtyCount;
		DirtyPaths.Add(Path);
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
			if (Package && !Package->IsDirty() && DirtyPaths.Contains(Path)) { ++Reloaded; }
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

FString UMCPEditorToolsToolset::AddEventParameter(const FString& BlueprintPath, const FString& EventName, const FString& ParamName, const FString& TypeName)
{
	if (ParamName.IsEmpty()) { return MCPToolUtils::Error(TEXT("ParamName is empty")); }
	FEdGraphPinType PinType;
	if (TypeName.Equals(TEXT("bool"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; }
	else if (TypeName.Equals(TEXT("int"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Int; }
	else if (TypeName.Equals(TEXT("float"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Real; PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double; }
	else if (TypeName.Equals(TEXT("byte"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Byte; }
	else if (TypeName.Equals(TEXT("string"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_String; }
	else if (TypeName.Equals(TEXT("name"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Name; }
	else if (TypeName.Equals(TEXT("text"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Text; }
	else if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Struct; PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get(); }
	else if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) { PinType.PinCategory = UEdGraphSchema_K2::PC_Struct; PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get(); }
	else if (TypeName.StartsWith(TEXT("/")))
	{
		UClass* Class = LoadObject<UClass>(nullptr, *TypeName);
		if (!Class) { return MCPToolUtils::Error(FString::Printf(TEXT("class '%s' not found"), *TypeName)); }
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = Class;
	}
	else
	{
		return MCPToolUtils::Error(FString::Printf(TEXT("unknown type '%s' (use bool, int, float, byte, string, name, text, Vector, Rotator or a class path)"), *TypeName));
	}

	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UK2Node_CustomEvent* Ev = MCPToolUtils::FindCustomEvent(BP, EventName);
	if (!Ev) { return MCPToolUtils::Error(FString::Printf(TEXT("custom event '%s' not found in %s"), *EventName, *BlueprintPath)); }
	if (Ev->UserDefinedPinExists(FName(*ParamName))) { return MCPToolUtils::Error(FString::Printf(TEXT("event '%s' already has a parameter '%s'"), *EventName, *ParamName)); }

	Ev->Modify();
	Ev->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, false);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return FString::Printf(TEXT("OK added %s (%s); event now has %d parameters (asset not saved)"), *ParamName, *TypeName, Ev->UserDefinedPins.Num());
}

static UEdGraphNode* FindNodeByName(UEdGraph* Graph, const FString& NodeName)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetName().Equals(NodeName, ESearchCase::IgnoreCase)) { return Node; }
	}
	return nullptr;
}

static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Dir)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == Dir && !Pin->bHidden && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase)) { return Pin; }
	}
	return nullptr;
}

static FString ListPins(UEdGraphNode* Node, EEdGraphPinDirection Dir)
{
	TArray<FString> Names;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction == Dir && !Pin->bHidden) { Names.Add(Pin->PinName.ToString()); }
	}
	return FString::Join(Names, TEXT(", "));
}

FString UMCPEditorToolsToolset::ConnectPins(const FString& BlueprintPath, const FString& GraphName, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UEdGraph* Graph = MCPToolUtils::FindGraph(BP, GraphName);
	if (!Graph) { return MCPToolUtils::Error(FString::Printf(TEXT("graph '%s' not found in %s"), *GraphName, *BlueprintPath)); }

	UEdGraphNode* A = FindNodeByName(Graph, FromNode);
	if (!A) { return MCPToolUtils::Error(FString::Printf(TEXT("node '%s' not found in graph %s"), *FromNode, *GraphName)); }
	UEdGraphNode* B = FindNodeByName(Graph, ToNode);
	if (!B) { return MCPToolUtils::Error(FString::Printf(TEXT("node '%s' not found in graph %s"), *ToNode, *GraphName)); }
	UEdGraphPin* PinA = FindPinByName(A, FromPin, EGPD_Output);
	if (!PinA) { return MCPToolUtils::Error(FString::Printf(TEXT("output pin '%s' not found on %s. Output pins: %s"), *FromPin, *FromNode, *ListPins(A, EGPD_Output))); }
	UEdGraphPin* PinB = FindPinByName(B, ToPin, EGPD_Input);
	if (!PinB) { return MCPToolUtils::Error(FString::Printf(TEXT("input pin '%s' not found on %s. Input pins: %s"), *ToPin, *ToNode, *ListPins(B, EGPD_Input))); }

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(PinA, PinB);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		return MCPToolUtils::Error(FString::Printf(TEXT("schema refuses %s.%s -> %s.%s: %s"), *FromNode, *FromPin, *ToNode, *ToPin, *Response.Message.ToString()));
	}
	if (!Schema->TryCreateConnection(PinA, PinB))
	{
		return MCPToolUtils::Error(FString::Printf(TEXT("connection %s.%s -> %s.%s could not be created"), *FromNode, *FromPin, *ToNode, *ToPin));
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return FString::Printf(TEXT("OK connected %s.%s -> %s.%s (asset not saved)"), *FromNode, *FromPin, *ToNode, *ToPin);
}

FString UMCPEditorToolsToolset::SetFunctionPure(const FString& BlueprintPath, const FString& FunctionName, bool bPure)
{
	FString Err;
	UBlueprint* BP = MCPToolUtils::LoadBlueprint(BlueprintPath, Err);
	if (!BP) { return Err; }
	UEdGraph* Graph = nullptr;
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetName().Equals(FunctionName, ESearchCase::IgnoreCase)) { Graph = G; break; }
	}
	if (!Graph) { return MCPToolUtils::Error(FString::Printf(TEXT("function '%s' not found in %s"), *FunctionName, *BlueprintPath)); }

	UK2Node_FunctionEntry* Entry = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		Entry = Cast<UK2Node_FunctionEntry>(Node);
		if (Entry) { break; }
	}
	if (!Entry) { return MCPToolUtils::Error(TEXT("function has no entry node")); }

	if (bPure)
	{
		if (UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then))
		{
			if (Then->LinkedTo.Num() > 0)
			{
				return MCPToolUtils::Error(TEXT("function still has execution flow (entry 'then' is linked). A pure function can only have data nodes: rewire first"));
			}
		}
		Entry->Modify();
		Entry->AddExtraFlags(FUNC_BlueprintPure | FUNC_BlueprintCallable);
	}
	else
	{
		Entry->Modify();
		Entry->ClearExtraFlags(FUNC_BlueprintPure);
		Entry->AddExtraFlags(FUNC_BlueprintCallable);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	const bool bNow = Entry->HasAnyExtraFlags(FUNC_BlueprintPure);
	return FString::Printf(TEXT("OK pure=%s (asset not saved)"), bNow ? TEXT("true") : TEXT("false"));
}

