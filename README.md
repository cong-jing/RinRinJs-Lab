# RinRinGame / RinRinJs

RinRinGame is an Unreal Engine 5.7 sample project built around `RinRinJs`, a runtime plugin that embeds Google V8 into Unreal Engine and allows C++ gameplay code to load, execute, and debug JavaScript.

This repository is intended primarily as portfolio/source-code material. It demonstrates my approach to native engine integration, third-party runtime embedding, Unreal module boundaries, error handling, and developer tooling. It is an exploratory project, not a maintained production plugin, and I do not provide compatibility guarantees or long-term support for external use.

Language versions:

- English: `README.md`
- Simplified Chinese: [README.zh-CN.md](README.zh-CN.md)
- Japanese: [README.ja.md](README.ja.md)

## Project Goal

The goal is to explore a JavaScript runtime layer for Unreal Engine that can eventually support:

- gameplay scripting from C++ through JavaScript;
- mod or user-authored script workflows;
- ES Module loading from project content;
- browser-based debugging through V8 Inspector / Chrome DevTools;
- structured C++ error propagation rather than log-only failure handling;
- a future bridge from JavaScript to Unreal objects, functions, and game systems.

The project currently focuses on the hard part first: getting V8 running inside a UE runtime module on Windows with MSVC. V8 is one of the more demanding engines to integrate in this environment because of build flags, CRT/linkage consistency, Unreal Build Tool interaction, and compiler/platform macro alignment. For that reason, the current supported runtime platform is Windows only.

## Current Status

Implemented:

- Unreal runtime plugin `RinRinJs`.
- V8 process initialization and shutdown.
- V8 isolate/context creation and cleanup.
- Direct JavaScript evaluation from C++.
- ES Module loading with dependency resolution.
- Example scripts under `Content/Mods/Core`.
- V8 Inspector integration over WebSocket for Chrome/Edge DevTools debugging.
- Local Inspector discovery endpoints such as `/json/list`.
- C++ wrappers for JS input/output values.
- Structured error/result primitives through `TExpected` and `FError`.
- A small Unreal `GameInstance` integration example.

Exploratory or incomplete:

- Package/mod registry and manifest loading.
- Stable public API for calling exported JS functions with typed arguments.
- UObject/UFunction binding.
- Async/promise integration with UE tick or latent actions.
- Cross-platform V8 builds.
- Packaging/distribution workflow for real game projects.

Not a goal right now:

- Marketplace-ready plugin packaging.
- Backward-compatible public API.
- Multi-platform binary distribution.
- Full JavaScript standard library or Node.js compatibility.

## Platform Support

Current target:

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- V8 linked as a monolithic static library

The repository includes V8 headers and a Windows release static library at:

```text
Plugins/RinRinJs/ThirdParty/v8
```

The V8 build was prepared for MSVC/Win64 with a monolithic static-library configuration. The important V8 build characteristics are:

```gn
is_component_build = false
is_debug = false
target_cpu = "x64"
target_os = "win"

v8_enable_i18n_support = false
v8_monolithic = true
v8_use_external_startup_data = false
v8_enable_pointer_compression = true
v8_jitless = false
v8_enable_sandbox = true

use_custom_libcxx = false
treat_warnings_as_errors = false
v8_symbol_level = 2
strip_debug_info = false
```

## Opening And Building

For normal use, this project should be treated as a standard Unreal Engine project. Building is handled by Unreal Engine and Unreal Build Tool, not by a specific IDE.

Recommended setup:

- Unreal Engine 5.7
- Visual Studio 2022 with C++ desktop toolchain
- Windows SDK compatible with the installed UE toolchain

Typical first-run flow after cloning:

1. Make sure the bundled V8 headers and Win64 library are present in `Plugins/RinRinJs/ThirdParty/v8`.
2. Open `RinRinGame.uproject` with Unreal Engine 5.7.
3. Let Unreal generate project files if prompted.
4. Build the project from Unreal Editor or let Unreal trigger the initial compile when opening the project.

Notes:

- Daily compilation can be done directly from Unreal Editor.
- Visual Studio and VSCode are optional development environments around the same UBT-based build pipeline.
- VSCode-specific project regeneration is only needed if you want VSCode workspace files, IntelliSense configuration, or editor tasks.

## How It Runs

The sample project starts the JavaScript runtime from `URinRinGameInstance`.

At startup:

1. `URinRinGameInstance::Init()` is called by Unreal.
2. It obtains the `RinRinJs` module through `FModuleManager`.
3. It calls `FRinRinJsModule::StartRuntime()`.
4. The plugin initializes V8 and creates an execution context.
5. The game loads the `"main"` JavaScript module.
6. The game evaluates a test expression, currently `foo(2, 3)`.

At shutdown:

1. `URinRinGameInstance::Shutdown()` obtains the plugin module again.
2. It calls `FRinRinJsModule::StopRuntime()`.
3. The plugin tears down loaded modules, Inspector, V8 context, isolate, allocator, and process-level V8 resources.

The current example resolver maps:

```text
main -> Content/Mods/Core/main.js
```

Relative ESM imports are resolved from the directory of the importing module.

## Basic Usage

The current public C++ entry point is `FRinRinJsModule`.

Example from the sample `GameInstance`:

```cpp
if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
{
    FRinRinJsModule& Module =
        FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");

    Module.StartRuntime();

    auto LoadResult = Module.LoadJsModule(
        "main",
        &URinRinGameInstance::resolveModulePath,
        &URinRinGameInstance::LoadJavascriptFile);

    if (!LoadResult)
    {
        LoadResult.Error().Log(LogTemp, ELogVerbosity::Error);
    }

    auto EvalResult = Module.EvaluateString("foo(2, 3)");
    if (!EvalResult)
    {
        EvalResult.Error().Log(LogTemp, ELogVerbosity::Error);
    }
}
```

The host game supplies two callbacks:

- `FResolveModuleIdFn`: converts an import specifier into a resolved module id, currently a normalized file path.
- `FLoadSourceByModuleIdFn`: reads the resolved module source as UTF-8 JavaScript.

This keeps file-system policy outside the V8 layer. The plugin does not assume that modules must always come from files; later this could support pak files, downloaded mod bundles, virtual file systems, or editor-managed assets.

## JavaScript Example

Current sample scripts live in:

```text
Content/Mods/Core
```

`main.js`:

```js
import { bar } from './utils.js';

function foo(a, b) {
    let x = bar(b);
    console.log('In foo: bar(', b, ')=', x);
    return a + bar(x);
}

console.log('In main.js: foo(1,2)=', foo(1, 2));

globalThis.foo = foo;

export { foo, bar };
```

`utils.js`:

```js
function bar(x) {
    return x * 2;
}

globalThis.bar = bar;

export { bar };
```

The `globalThis.foo = foo` line exists because the current direct evaluation path can call global functions after the ESM entry module has been evaluated. The newer internal direction is to call module exports directly through a typed C++ API.

## Debugging With Chrome DevTools

The plugin starts a V8 Inspector transport when the V8 context is created.

Default local endpoint:

```text
ws://127.0.0.1:9229/
```

Discovery endpoints:

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

Typical workflow:

1. Run the Unreal project on Windows.
2. Open Chrome or Edge.
3. Navigate to `chrome://inspect`.
4. Configure or inspect `127.0.0.1:9229`.
5. Attach DevTools to the exposed V8 target.

The Inspector transport is intentionally local-only by default. It uses CivetWeb for HTTP/WebSocket handling and pumps Chrome DevTools Protocol messages through Unreal's ticker.

## Project Map

For a more detailed source tree, layer breakdown, and dependency flow, see:

- English: [docs/project-map.md](docs/project-map.md)
- Simplified Chinese: [docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- Japanese: [docs/project-map.ja.md](docs/project-map.ja.md)

## Design Notes

The implementation intentionally keeps several boundaries visible:

- Unreal-facing APIs are kept in `Public`.
- V8-specific types are mostly kept in `Private`.
- The host game owns module resolution and source loading policy.
- Runtime errors are returned through `TExpected` rather than only printed.
- Browser debugging is treated as part of the runtime experience, not as an afterthought.

This shape is useful for a portfolio project because it shows not just the final feature, but also the engineering concerns around embedding a complex native dependency into a large engine.

## Future Work

Planned or likely next steps:

- Finish the package/mod registry and manifest format.
- Expose a stable `ExecuteJsFunction` API from `FRinRinJsModule`.
- Call ES Module exports directly instead of relying on globals.
- Expand `FValueIntoJs` / `FValueFromJs` to support arrays, objects, and Unreal references.
- Add UObject/UFunction binding.
- Integrate promise/microtask behavior with Unreal tick semantics.
- Improve source-map support for TypeScript workflows.
- Clean up legacy ChakraCore code or move it to documentation/history.
- Add automated compile/smoke-test documentation for Windows.
