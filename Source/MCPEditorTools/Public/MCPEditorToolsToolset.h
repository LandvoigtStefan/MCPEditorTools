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
	 * @param GraphName Name of the graph, e.g. EventGraph or a function name
	 * @param bDryRun true = only list (default behaviour you should use first); false = remove the listed nodes, recompile (asset not saved)
	 * @return First line "dry_run=<bool> candidates=<n> removed=<n>", then one line per node: Name | Class | Title | X,Y; or "ERROR: ..."
	 */
	UFUNCTION(meta = (AICallable))
	static FString FindUnconnectedNodes(const FString& BlueprintPath, const FString& GraphName, bool bDryRun);
};
