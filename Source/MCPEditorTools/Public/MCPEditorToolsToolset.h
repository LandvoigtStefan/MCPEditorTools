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
};
