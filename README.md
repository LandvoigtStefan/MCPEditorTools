# MCPEditorTools

Small, generic, editor-only toolset for Unreal Engine 5.8 that closes gaps in Epic's `unreal-mcp` tools. One tool = one job. No project-specific paths or classes: every tool takes asset paths or names as parameters. This README holds the ONE tool list.

## Requirements and build
- UE 5.8 (uses the experimental `ToolsetRegistry` / `UToolsetDefinition`), Visual Studio 2022 with the C++ game workload (the plugin has source, no committed binaries).
- Editor module only: list it in the `.uproject` with `"TargetAllowList": ["Editor"]` so it never ends up in a packaged game.
- Build with the editor closed: `Build.bat <Project>Editor Win64 Development -Project=<path>.uproject -WaitMutex`.
- Runs only on `127.0.0.1` through the editor's MCP server; no network code in the plugin.

## Usage
Call with `toolset_name="MCPEditorTools.MCPEditorToolsToolset"` and the short tool name, e.g. `call_tool(tool_name="ConnectPins", ...)`. Arguments are camelCase. Errors come back as a string starting with `ERROR:`. Tools never save: changed assets stay dirty in memory (save them yourself). Tools that delete or discard have a `bDryRun` parameter: use `true` first.

## Tools
| Tool | What it does | Parameters | Risk |
|---|---|---|---|
| Ping | health check, proves the toolset is registered | none | none |
| GetEventReplication | reads a custom event's replication (mode + reliable) | blueprintPath, eventName | read-only |
| SetEventReplication | sets NotReplicated / Server / Client / Multicast + reliable on a custom event, recompiles | blueprintPath, eventName, mode, bReliable | changes a Blueprint (dirty, not saved) |
| ListCustomEvents | lists custom events + replication of one Blueprint or all Blueprints below a folder | path (asset or folder) | read-only (loads many Blueprints) |
| FindUnconnectedNodes | lists nodes with NO connected pin in a graph (never comment/entry/result; events only listed); bDryRun=false removes them + recompiles | blueprintPath, graphName (or *), bDryRun | deletes nodes: dry run first |
| FindDeadNodes | finds nodes that can never run or feed a running node (unconnected, self-connected clusters, unused pure nodes); bDryRun=false removes them + recompiles | blueprintPath, graphName (or *), bDryRun | deletes nodes: dry run first, run tests after |
| AddEventParameter | adds a typed INPUT parameter to a custom event (so a Server RPC can carry data); recompiles, not saved | blueprintPath, eventName, paramName, typeName (bool/int/float/byte/string/name/text/Vector/Rotator or a class path) | changes a Blueprint (dirty, not saved) |
| AddInterfaceFunction | adds a function (typed inputs/outputs) to a Blueprint Interface; recompiles, not saved | interfacePath, functionName, inputs and outputs as `Name:Type;Name:Type` (types: bool/int/float/byte/string/name/text/Vector/Rotator/Transform or a class/struct/enum path) | changes the interface (dirty, not saved) |
| ConformInterfaces | creates the missing function graphs in a Blueprint for the interface functions it implements (use after AddInterfaceFunction); recompiles, not saved | blueprintPath | changes a Blueprint (dirty, not saved) |
| AddInterfaceToBlueprint | makes a Blueprint implement a Blueprint Interface (creates the function graphs for functions with outputs; functions without outputs become events: add by hand); recompiles, not saved; already implemented = no change | blueprintPath, interfacePath | changes a Blueprint (dirty, not saved) |
| ConnectPins | connects an output pin to an input pin by node name + pin name; fails loudly (lists real pins, gives the schema's reason) | blueprintPath, graphName, fromNode, fromPin, toNode, toPin | changes a graph (dirty, not saved) |
| SetFunctionPure | makes a function pure / impure and recompiles; refuses to make a function pure while it still has execution flow | blueprintPath, functionName, bPure | changes a Blueprint (dirty, not saved) |
| ReloadOrDiscardAsset | reloads loaded assets from disk and DROPS their unsaved changes (replaces save, close editor, git restore); skips clean / not-loaded / not-on-disk assets | assetPaths (array of /Game/... package paths), bDryRun | destroys unsaved work: only for assets you did not really edit |

## Tests
Automation group `MCPEditorTools.*` (temporary in-memory assets under `/Game/DoN/Tests/Temp/`, never saved). Run: `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MCPEditorTools"` or through the project's headless runner. Every new test must be shown failing once before it counts.
