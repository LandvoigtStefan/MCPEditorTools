#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CustomEvent.h"

/** Small shared helpers for the tools. Errors are returned as strings starting with "ERROR:". */
namespace MCPToolUtils
{
	inline FString Error(const FString& Msg) { return TEXT("ERROR: ") + Msg; }

	/** Load a Blueprint from "/Game/Folder/BP_Name" or "/Game/Folder/BP_Name.BP_Name". */
	inline UBlueprint* LoadBlueprint(const FString& Path, FString& OutError)
	{
		FString ObjectPath = Path;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			ObjectPath += TEXT(".") + FPackageName::GetShortName(Path);
		}
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ObjectPath);
		if (!BP)
		{
			OutError = Error(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
		}
		return BP;
	}

	inline void GetAllGraphs(UBlueprint* BP, TArray<UEdGraph*>& Out)
	{
		BP->GetAllGraphs(Out);
	}

	/** Find a custom event node by its custom function name in all graphs of the Blueprint. */
	inline UK2Node_CustomEvent* FindCustomEvent(UBlueprint* BP, const FString& EventName)
	{
		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_CustomEvent* Ev = Cast<UK2Node_CustomEvent>(Node))
				{
					if (Ev->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
					{
						return Ev;
					}
				}
			}
		}
		return nullptr;
	}
}
