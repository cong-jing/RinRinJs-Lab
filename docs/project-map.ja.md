# Project Map

This document summarizes the current source layout and dependency flow of `RinRinJs-Lab` and the `RinRinJs` plugin.

Language versions:

- English: [docs/project-map.md](project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](project-map.zh-CN.md)
- 日本語: `docs/project-map.ja.md`

## Top Level

```text
RinRinJsLab.uproject
Config/
Source/RinRinJsLab/
Content/Mods/Core/
Plugins/RinRinJs/
```

- `Source/RinRinJsLab` contains the host game module.
- `Content/Mods/Core` contains the JavaScript script package.
- `Plugins/RinRinJs` contains the runtime plugin that embeds V8.

## Host Game Module

```text
Source/RinRinJsLab/
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
  RinRinJsLab.Build.cs
```

`URinRinJsLabGameInstance` starts the runtime, waits until the game world has begun play, loads `Content/Mods/Core`, and ticks JavaScript every frame.

```text
GameInstance
  -> FRinRinJsModule::StartRuntime()
  -> SetGameWorld(World)
  -> LoadScriptPackage(Content/Mods/Core)
  -> TickRuntime(dt)
```

## Script Package

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

The manifest points to `main.js`, which exports:

```text
start(context)
tick(deltaSeconds)
dispose()
```

## Plugin API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
```

`FRinRinJsModule` is the Unreal-facing entry point. Public headers avoid exposing V8 details.

## Runtime Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/
  ScriptHost.h
  ScriptHost.cpp
  ScriptManifest.h
  ScriptManifest.cpp
```

`FScriptHost` owns one script package at a time. It loads the manifest, injects the bridge, loads the main ES module, calls lifecycle exports, and unloads JS-created actors.

## Bridge Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/
  NativeBridge.h
  NativeBridge.cpp
  ActorHandleRegistry.h
  ActorHandleRegistry.cpp
```

The bridge injects `globalThis.ue` and exposes a small allowlisted API:

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

Actors are exposed to JavaScript through opaque integer handles.

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

`FV8Loader` owns V8 process/context lifecycle. `FV8ModuleManager` loads ES Modules and calls exported functions.

## Inspector Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/Inspector/
```

The Inspector layer exposes Chrome DevTools Protocol through a local CivetWeb HTTP/WebSocket transport.

## Overall Flow

```text
RinRinJsLab GameInstance
  -> FRinRinJsModule
    -> FScriptHost
      -> FNativeBridge
        -> FActorHandleRegistry
      -> FV8Loader
        -> FV8ModuleManager
        -> FV8Inspector
          -> CivetWeb
      -> TExpected / FError
```

## Notes

- `Content/Mods/Core/main.js` is the main script iteration point.
- `RinRinJs.Reload` reloads the active script package.
- Script imports are clamped to the package root.
- World-sensitive script loading happens after the game world has begun play.
