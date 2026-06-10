# Project Map

This document describes the current source layout and dependency flow of `RinRinJs-Lab` and the `RinRinJs` plugin.

Language versions:

- English: `docs/project-map.md`
- Simplified Chinese: [docs/project-map.zh-CN.md](project-map.zh-CN.md)
- Japanese: [docs/project-map.ja.md](project-map.ja.md)

## Top Level

```text
RinRinJsLab.uproject
Config/
Source/RinRinJsLab/
Content/Mods/Core/
Plugins/RinRinJs/
```

Responsibilities:

- `RinRinJsLab.uproject` defines the Unreal project.
- `Config/DefaultEngine.ini` sets the sample map and `URinRinJsLabGameInstance`.
- `Source/RinRinJsLab` contains the host game module.
- `Content/Mods/Core` contains the script package loaded by the runtime.
- `Plugins/RinRinJs` contains the runtime plugin that embeds V8.

## Host Game Module

```text
Source/RinRinJsLab/
  RinRinJsLab.Build.cs
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
```

Responsibilities:

- Depends on the `RinRinJs` plugin module.
- Starts and stops the plugin runtime.
- Registers a ticker that updates the plugin world pointer and drives script tick.
- Waits until the game world has begun play before loading the script package.
- Loads `Content/Mods/Core` as the active package.

Current flow:

```text
URinRinJsLabGameInstance::Init()
  -> FRinRinJsModule::StartRuntime()
  -> register FTSTicker

TickScripts(dt)
  -> FRinRinJsModule::SetGameWorld(World)
  -> if World->IsGameWorld() && World->HasBegunPlay():
       LoadScriptPackage(Content/Mods/Core)
  -> TickRuntime(dt)

Shutdown()
  -> UnloadScriptPackage()
  -> StopRuntime()
```

## Script Package

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

Responsibilities:

- `rinrin.manifest.json` defines package metadata and the main module.
- `package.json` marks the directory as ESM for editor/tooling support.
- `main.js` exports lifecycle functions.

Manifest:

```json
{
    "name": "core-demo",
    "version": "0.1.0",
    "main": "main.js"
}
```

Lifecycle exports:

```text
start(context)
tick(deltaSeconds)
dispose()
```

## Plugin Descriptor And Build

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

Responsibilities:

- Declares `RinRinJs` as a runtime plugin module.
- Adds Unreal dependencies such as `Core`, `CoreUObject`, `Engine`, `Json`, and `Projects`.
- Adds V8 include/library paths.
- Defines V8 compile macros matching the bundled Win64 static build.
- Adds CivetWeb definitions for the local Inspector HTTP/WebSocket transport.

## Plugin Public API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
    Error.h
    Expected.h
    Log.h
```

Responsibilities:

- `RinRinJs.h` declares `FRinRinJsModule`, the Unreal-facing plugin entry point.
- `ModuleResolver.h` declares host-provided module resolve/load callback types.
- `Util/Expected.h` defines success-or-error return values.
- `Util/Error.h` defines structured errors with source location and optional JS stack information.
- `Util/Log.h` defines plugin logging helpers.

Important boundary:

- Public headers avoid exposing V8 types.
- Runtime and bridge implementation details stay in `Private`.

## Plugin Module

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

Responsibilities:

- Implements `FRinRinJsModule`.
- Initializes process-level V8 state during module startup.
- Starts and stops execution context state through explicit runtime calls.
- Exposes script package lifecycle to the host game.
- Registers and unregisters the `RinRinJs.Reload` console command.

Public module calls used by the host:

```text
StartRuntime()
StopRuntime()
SetGameWorld(UWorld*)
LoadScriptPackage(packageRoot)
UnloadScriptPackage()
ReloadScriptPackage()
TickRuntime(deltaSeconds)
```

## Runtime Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/
  ScriptHost.h
  ScriptHost.cpp
  ScriptManifest.h
  ScriptManifest.cpp
```

Responsibilities:

- Load `rinrin.manifest.json`.
- Normalize package root and main module paths.
- Clamp module resolution to the package root.
- Create the per-package native bridge and actor registry.
- Inject `globalThis.ue` into the V8 context.
- Load and evaluate the main ES module.
- Call `start(context)`, `tick(deltaSeconds)`, and `dispose()` when exported.
- Rebuild the V8 execution context during reload.
- Destroy JS-created actors during unload/reload.

Flow:

```text
FRinRinJsModule
  -> FScriptHost
    -> LoadScriptManifest
    -> FNativeBridge
    -> FActorHandleRegistry
    -> FV8Loader
    -> FV8ModuleManager
```

## Bridge Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/
  NativeBridge.h
  NativeBridge.cpp
  ActorHandleRegistry.h
  ActorHandleRegistry.cpp
```

Responsibilities:

- Inject the `ue` object into JavaScript.
- Validate JS arguments before calling Unreal APIs.
- Convert simple JS objects to `FVector`, `FRotator`, and `FTransform`.
- Spawn `AStaticMeshActor` from a `UStaticMesh` asset path.
- Name spawned actors with the `RinRinJsDemoActor` prefix for easy Outliner lookup.
- Store actor references behind opaque integer handles.
- Resolve handles back to actors for later bridge calls.
- Destroy all registered JS-created actors on unload.

Current bridge API:

```text
ue.log(...)
ue.spawnActorByPath(assetPath, transform)
ue.destroy(actorHandle)
ue.setLocation(actorHandle, location)
ue.getLocation(actorHandle)
ue.setRotation(actorHandle, rotation)
ue.setTransform(actorHandle, transform)
ue.addWorldOffset(actorHandle, offset)
ue.setVisible(actorHandle, visible)
```

## V8 Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Loader.h
  V8Loader.cpp
  V8ModuleManager.h
  V8ModuleManager.cpp
  V8Console.h
  V8Console.cpp
```

Responsibilities:

- Initialize and finalize process-level V8 resources.
- Own the V8 platform, allocator, isolate, and execution context.
- Create and destroy the V8 context.
- Create and expose `FV8ModuleManager`.
- Execute direct JavaScript source strings.
- Load ES Modules through host resolver/source-loader callbacks.
- Execute exported module functions.
- Keep V8-specific types out of public plugin headers.

Conceptual flow:

```text
FScriptHost
  -> FV8Loader
    -> v8::Platform
    -> v8::Isolate
    -> v8::Context
    -> FV8ModuleManager
      -> CompileModule
      -> InstantiateModule
      -> Evaluate
      -> ExecuteFunction(exportName)
```

## Inspector Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/Inspector/
  V8Inspector.h
  V8Inspector.cpp
  V8InspectorHost.h
  V8InspectorHost.cpp
  V8InspectorClient.cpp
  V8InspectorTransport.h
  V8InspectorTransport.cpp
  V8InspectorUtil.h
```

Responsibilities:

- Expose Chrome DevTools Protocol over local WebSocket.
- Provide `/json`, `/json/list`, and `/json/version` discovery endpoints.
- Create and manage V8 Inspector sessions.
- Bridge DevTools messages into V8.
- Route JS console messages to Unreal logs.
- Pump transport messages from Unreal's ticker.

Dependency flow:

```text
FV8Loader
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## Utility Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

Responsibilities:

- Return failures through `TExpected`.
- Preserve structured error context in `FError`.
- Capture and print JS stack details when available.
- Format logs with source location information.

## Third-Party Dependencies

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/Source/ChakraCoreLoader
```

Responsibilities:

- V8 provides the JavaScript engine and Inspector API.
- CivetWeb provides local HTTP/WebSocket transport for DevTools.
- ChakraCoreLoader remains in the repository as earlier runtime-loader work and is not the active path when `RinRinJs_USE_V8=1`.

## Overall Dependency Flow

```text
RinRinJsLab GameInstance
  -> FRinRinJsModule
    -> FScriptHost
      -> ScriptManifest
      -> FNativeBridge
        -> FActorHandleRegistry
        -> UWorld / AStaticMeshActor
      -> FV8Loader
        -> V8 process/platform
        -> V8 isolate/context
        -> FV8ModuleManager
          -> host module resolver
          -> host source loader
          -> exported lifecycle calls
        -> FV8Inspector
          -> FV8InspectorHost
          -> FV8InspectorTransport
            -> CivetWeb
      -> Error utilities
        -> TExpected
        -> FError
```

## Development Notes

- `Content/Mods/Core/main.js` is the fastest place to iterate on script behavior.
- `RinRinJs.Reload` is the current runtime reload entry point.
- Actor handles are intentionally opaque. JavaScript should not receive raw UObject pointers.
- Script loading should continue to reject imports outside the package root.
- World-sensitive script loading should happen after the game world has begun play.
