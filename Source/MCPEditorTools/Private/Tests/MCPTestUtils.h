#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CustomEvent.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

/** Helpers for the automation tests: temporary in-memory assets (never saved to disk) under /Game/DoN/Tests/Temp/. */
namespace MCPTestUtils
{
	static const TCHAR* TempFolder = TEXT("/Game/DoN/Tests/Temp/");

	inline UBlueprint* CreateTempActorBlueprint(const FString& Name)
	{
		const FString PackagePath = FString(TempFolder) + Name;
		UPackage* Package = CreatePackage(*PackagePath);
		return FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}

	inline FString PathOf(UBlueprint* BP) { return BP->GetPackage()->GetName(); }

	inline UK2Node_CustomEvent* AddCustomEvent(UBlueprint* BP, const FString& EventName)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
		UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
		Node->CustomFunctionName = FName(*EventName);
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}

	/** Saves the temp Blueprint to disk under /Game/DoN/Tests/Temp/ (needed for reload tests). Returns the file name. Delete it with DeleteTempFile. */
	inline FString SaveTempToDisk(UBlueprint* BP)
	{
		const FString File = FPackageName::LongPackageNameToFilename(PathOf(BP), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(BP->GetPackage(), BP, *File, Args);
		return File;
	}

	inline void DeleteTempFile(const FString& File)
	{
		if (File.Contains(TEXT("DoN/Tests/Temp"))) { IFileManager::Get().Delete(*File); } // safety: only ever delete inside the temp folder
	}

	inline void Cleanup(UBlueprint* BP)
	{
		if (!BP) { return; }
		UPackage* Package = BP->GetPackage();
		BP->ClearFlags(RF_Public | RF_Standalone);
		BP->MarkAsGarbage();
		if (Package) { Package->MarkAsGarbage(); }
	}
}
