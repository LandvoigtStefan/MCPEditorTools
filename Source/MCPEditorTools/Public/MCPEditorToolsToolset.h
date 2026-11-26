#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "MCPEditorToolsToolset.generated.h"

/** Generic editor tools. One UFUNCTION = one job. Every tool takes asset paths/names as parameters. */
UCLASS()
class MCPEDITORTOOLS_API UMCPEditorToolsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Health check: returns "pong" plus the plugin version, proves the toolset is registered. */
	UFUNCTION(meta = (AICallable))
	static FString Ping();

	/**
	 * Reads the replication setting of a custom event in a Blueprint.
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param EventName Name of the custom event
	 * @return "mode=<NotReplicated|Server|Client|Multicast> reliable=<true|false>" or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString GetEventReplication(const FString& BlueprintPath, const FString& EventName);

	/**
	 * Discards unsaved in-memory changes of loaded assets by reloading them from disk (replaces "save, close editor, git restore").
	 * DESTROYS unsaved work in those assets: use only for assets you did not really edit (e.g. dirty just from reading) or want to throw away.
	 * Assets that are clean, not loaded, or not on disk are skipped and reported.
	 * @param AssetPaths Package paths, e.g. /Game/Folder/BP_Name
	 * @param bDryRun true = only report which assets are dirty and would be reloaded (use first); false = reload the dirty ones
	 * @return One line per asset ("<path> | dirty | reloaded" etc.), first line "dry_run=<bool> dirty=<n> reloaded=<n>"; or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReloadOrDiscardAsset(const TArray<FString>& AssetPaths, bool bDryRun);

	/**
	 * Lists all custom events with their replication setting. Read-only.
	 * @param Path A Blueprint asset path, or a folder (e.g. /Game/DoN) to scan all Blueprints below it
	 * @return One line per event: "<BlueprintPath> | <EventName> | mode=... reliable=..." (plus a first line with the count), or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString ListCustomEvents(const FString& Path);

	/**
	 * Sets the replication of a custom event. Does NOT save the asset (it stays dirty in memory).
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param EventName Name of the custom event
	 * @param Mode One of NotReplicated, Server, Client, Multicast
	 * @param bReliable Reliable RPC (ignored for NotReplicated)
	 * @return Confirmation with the new state, or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString SetEventReplication(const FString& BlueprintPath, const FString& EventName, const FString& Mode, bool bReliable);

	/**
	 * Finds nodes that have NO connection at all (no linked pin) in one graph of a Blueprint, and optionally removes them.
	 * Never touches comment boxes, function entry/result nodes or event nodes (events are only listed as "[event, kept]").
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param GraphName Name of the graph, e.g. EventGraph or a function name, or "*" for ALL graphs of the Blueprint
	 * @param bDryRun true = only list (default behaviour you should use first); false = remove the listed nodes, recompile (asset not saved)
	 * @return First line "dry_run=<bool> candidates=<n> removed=<n>", then one line per node: Graph | Name | Class | Title | X,Y; or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString FindUnconnectedNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun);

	/**
	 * Finds DEAD nodes: nodes that can never run or feed a running node. A node is live if it is an event, function entry or result,
	 * is reached by execution flow from a live node, or is a data source (input pin link) of a live node. Everything else is dead,
	 * including clusters connected to each other and unused pure nodes (e.g. leftover "bind" values). Never touches comment boxes.
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param GraphName Graph name, or "*" for ALL graphs
	 * @param bDryRun true = only list; false = remove the dead nodes and recompile (asset not saved)
	 * @return First line "dry_run=<bool> dead=<n> removed=<n>", then one line per node: Graph | Name | Class | Title | X,Y; or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString FindDeadNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun);

	/**
	 * Connects an OUTPUT pin of one node to an INPUT pin of another node in one graph. Fails loudly: unknown graph, node or pin
	 * (the error lists the available pins), or a connection the Blueprint schema refuses (the error contains the schema's reason).
	 * Does not break existing links unless the schema itself replaces them (e.g. a second link into a single-input data pin).
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param GraphName Graph name (EventGraph or a function name)
	 * @param FromNode Node name as printed by FindUnconnectedNodes / FindDeadNodes (e.g. K2Node_CallFunction_12)
	 * @param FromPin Name of the OUTPUT pin on FromNode (e.g. then, ReturnValue)
	 * @param ToNode Node name of the target node
	 * @param ToPin Name of the INPUT pin on ToNode (e.g. execute, InString)
	 * @return "OK connected A.pin -> B.pin (asset not saved)" or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString ConnectPins(const FString& BlueprintPath, const FString& GraphName, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin);

	/**
	 * Sets a Blueprint function "Pure" (no exec pins, only data) or back to impure. Refuses and reports if the function body still
	 * has an execution flow that a pure function cannot have (entry's exec output linked) when setting Pure.
	 * @param BlueprintPath Asset path, e.g. /Game/Folder/BP_Name
	 * @param FunctionName Name of the function graph
	 * @param bPure true = make pure, false = make impure
	 * @return "OK pure=<bool> (asset not saved)" or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString SetFunctionPure(const FString& BlueprintPath, const FString& FunctionName, bool bPure);
};
