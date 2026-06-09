# Project Map

This document describes the main source layout and dependency flow of `RinRinJs-Lab` and the `RinRinJs` plugin.

Language versions:

- English: `docs/project-map.md`
- 简体中文: [docs/project-map.zh-CN.md](project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](project-map.ja.md)

## Top Level

```text
RinRinJsLab.uproject
Source/RinRinJsLab
Content/Mods/Core
Plugins/RinRinJs
```

Responsibilities:

- `RinRinJsLab.uproject` defines the Unreal project.
- `Source/RinRinJsLab` contains the sample game module.
- `Content/Mods/Core` contains JavaScript examples loaded by the runtime.
- `Plugins/RinRinJs` contains the Unreal plugin that embeds V8.

## Game Module

```text
Source/RinRinJsLab/
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
  RinRinJsLab.Build.cs
```

Responsibilities:

- Owns the sample startup/shutdown flow.
- Calls into `FRinRinJsModule`.
- Provides module path resolution.
- Provides JavaScript source loading.
- Demonstrates how gameplay code can call into the plugin.

Current dependency:

```text
RinRinJsLab.Build.cs
  -> PrivateDependencyModuleNames
    -> RinRinJs
```

## Plugin Descriptor And Build

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

Responsibilities:

- Declares `RinRinJs` as a runtime plugin module.
- Adds Unreal dependencies such as `Core`, `CoreUObject`, and `Projects`.
- Adds V8 include and library paths.
- Adds V8 compile definitions matching the bundled Win64 static build.
- Adds CivetWeb compile definitions for WebSocket support.

## Plugin Public API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
  Value/
```

Responsibilities:

- `RinRinJs.h`: declares `FRinRinJsModule`, the main Unreal-facing plugin API.
- `ModuleResolver.h`: declares host-provided module resolve/load callback types.
- `Value/ValueIntoJs.h`: defines C++ values that can be passed into JS.
- `Value/ValueFromJs.h`: wraps JS return values for C++ callers.
- `Util/Expected.h`: defines success-or-error return values.
- `Util/Error.h`: defines structured errors with source location and optional stack information.
- `Util/Log.h`: defines plugin logging helpers.

Important boundary:

- Public headers should avoid leaking V8 types where possible.
- V8-specific code should remain in `Private`, behind stable plugin-facing wrappers.

## Plugin Module Implementation

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

Responsibilities:

- Implements `FRinRinJsModule`.
- Initializes process-level V8 state during module startup.
- Creates and destroys execution runtime state on explicit start/stop.
- Exposes module loading and direct script evaluation to game code.

## V8 Runtime Layer

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Runtime.h
  V8Runtime.cpp
  V8EsModuleLoader.h
  V8EsModuleLoader.cpp
  V8Includes.h
```

Responsibilities:

- Own V8 platform initialization.
- Own V8 isolate/context lifecycle.
- Evaluate direct JavaScript source strings.
- Load and evaluate ES Modules.
- Cache compiled modules.
- Convert V8 values to engine-facing wrappers.

Conceptual flow:

```text
FRinRinJsModule
  -> FV8Runtime
    -> v8::Platform
    -> v8::Isolate
    -> v8::Context
    -> ES module loader
```

## ES Module Loader

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8EsModuleLoader.*
```

Responsibilities:

- Resolve module specifiers through host callbacks.
- Load JavaScript source through host callbacks.
- Compile modules through `v8::ScriptCompiler::CompileModule`.
- Instantiate modules with `v8::Module::InstantiateModule`.
- Evaluate modules with `v8::Module::Evaluate`.
- Cache modules by resolved id.
- Track referrer module ids for relative import resolution.

Host callbacks:

```text
FResolveModuleIdFn
  (referrer id, import specifier) -> resolved module id

FLoadSourceByModuleIdFn
  (resolved module id) -> UTF-8 source
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
FV8Runtime
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## Value And Utility Layers

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Value/
Plugins/RinRinJs/Source/RinRinJs/Private/Value/
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

Responsibilities:

- Convert between V8 values and public value wrappers.
- Keep V8 handles and context details in private implementation types.
- Return failures through `TExpected`.
- Preserve structured error details in `FError`.
- Format logs with source location information.

Current value types:

```text
FValueIntoJs
  Undefined
  Null
  Bool
  Int32
  Double
  String

FValueFromJs
  Undefined
  Null
  Bool
  Int32
  Double
  String
```

## Package Registry Work Area

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  PackageLoader.h
  PackageRegistry.h
  PackageRegistry.cpp
```

Current intent:

- Discover package manifests.
- Cache package metadata.
- Sort packages by load order.
- Delegate package loading to a loader interface.

This area is exploratory and not yet a stable runtime feature.

## Third-Party Dependencies

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/ThirdParty/ChakraCore
```

Responsibilities:

- V8 provides the JavaScript engine and Inspector API.
- CivetWeb provides local HTTP/WebSocket transport for DevTools.
- ChakraCore is retained as earlier exploration and fallback material.

## Overall Dependency Flow

```text
RinRinJsLab GameInstance
  -> FRinRinJsModule
    -> FV8Runtime
      -> V8 process/platform
      -> V8 isolate/context
      -> ES module loader
        -> host module resolver
        -> host source loader
      -> FV8Inspector
        -> V8 Inspector Host
        -> V8 Inspector Transport
          -> CivetWeb
      -> Value converters
        -> FValueIntoJs
        -> FValueFromJs
      -> Error utilities
        -> TExpected
        -> FError
```
