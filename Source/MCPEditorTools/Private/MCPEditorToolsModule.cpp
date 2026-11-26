#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "MCPEditorToolsToolset.h"

class FMCPEditorToolsModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UMCPEditorToolsToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UToolsetRegistry::UnregisterToolsetClass(UMCPEditorToolsToolset::StaticClass());
		}
	}
};

IMPLEMENT_MODULE(FMCPEditorToolsModule, MCPEditorTools)
