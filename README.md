# RinRinJs-Lab / RinRinJs

Language versions:

- English: `README.md`
- 简体中文： [README.zh-CN.md](README.zh-CN.md)
- 日本語: [README.ja.md](README.ja.md)

## Maintenance Notice

This repository is being kept online primarily for portfolio and hiring review. I cannot provide ongoing maintenance, feature commitments, or external support for it at this time.

RinRinJs-Lab is an Unreal Engine 5.7 sample project built around `RinRinJs`, a runtime plugin that embeds Google V8 into Unreal Engine. The project can load an ES Module script package from project content, expose a small Unreal-facing JavaScript bridge, drive script lifecycle functions from the game loop, and debug JavaScript through Chrome/Edge DevTools.

This repository is primarily portfolio/source-code material. It shows native engine integration, third-party runtime embedding, Unreal module boundaries, structured error handling, browser debugging, and a script-driven gameplay loop.

Demo video: JavaScript editing, Unreal runtime playback, hot reload, and Chrome DevTools debugging.


https://github.com/user-attachments/assets/be83b3a8-c648-420e-9dd5-5a582635e4c4

## Current Status

Implemented:

- Unreal runtime plugin module `RinRinJs`.
- V8 process initialization and shutdown.
- V8 isolate/context creation and cleanup.
- Direct JavaScript evaluation from C++.
- ES Module loading with dependency resolution and module cache handling.
- Script package loading through `rinrin.manifest.json`.
- Script lifecycle: `start(context)`, `tick(deltaSeconds)`, and `dispose()`.
- `URinRinJsLabGameInstance` integration that starts the runtime, waits for the game world to begin play, loads the script package, and ticks JS every frame.
- A small native `ue` bridge injected into JavaScript.
- Actor creation/control through opaque actor handles.
- Runtime reload through the `RinRinJs.Reload` console command.
- V8 Inspector integration over WebSocket for Chrome/Edge DevTools.
- Local Inspector discovery endpoints such as `/json/list`.
- Structured error/result primitives through `TExpected` and `FError`.

Current JavaScript bridge:

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

Not currently provided:

- Stable public API compatibility guarantees.
- Marketplace-ready plugin packaging.
- Node.js compatibility.
- Cross-platform V8 binaries.
- General UObject/UFunction reflection.
- A complete package/mod manager.

## Platform Support

Current target:

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- V8 linked as a monolithic static library

The repository expects V8 headers and the Win64 release static library at:

```text
Plugins/RinRinJs/ThirdParty/v8
```

## Opening And Building

Recommended setup:

- Unreal Engine 5.7
- Visual Studio 2022 with the C++ desktop toolchain
- Windows SDK compatible with the installed UE toolchain

Typical first-run flow:

1. Confirm that `Plugins/RinRinJs/ThirdParty/v8` contains the bundled V8 headers and Win64 library.
2. Open `RinRinJsLab.uproject` with Unreal Engine 5.7.
3. Let Unreal generate project files if prompted.
4. Build from Unreal Editor, Visual Studio, or Unreal Build Tool.

## Runtime Flow

The game module starts and drives the runtime from `URinRinJsLabGameInstance`.

Startup flow:

1. `URinRinJsLabGameInstance::Init()` obtains the `RinRinJs` module.
2. The game calls `FRinRinJsModule::StartRuntime()`.
3. The plugin initializes V8 and creates an execution context.
4. The game ticker updates the plugin world pointer.
5. Once the world is a game world and `HasBegunPlay()` is true, the game loads `Content/Mods/Core`.
6. `FScriptHost` reads `rinrin.manifest.json`, loads the manifest `main` module, injects `globalThis.ue`, and calls `start({ packageName })`.
7. Each tick calls exported `tick(deltaSeconds)` when present.

Shutdown flow:

1. `URinRinJsLabGameInstance::Shutdown()` removes the ticker.
2. The game unloads the active script package.
3. `FScriptHost` calls `dispose()` when exported, destroys JS-spawned actors, and clears actor handles.
4. The plugin stops the runtime and tears down V8 context/process state.

## Script Package

The current script package lives at:

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

`rinrin.manifest.json`:

```json
{
    "name": "core-demo",
    "version": "0.1.0",
    "main": "main.js"
}
```

`main.js` exports lifecycle functions:

```js
export function start(context) {
    actor = ue.spawnActorByPath("/Engine/BasicShapes/Cube.Cube", {
        location: { x: 200, y: 0, z: 120 },
        rotation: { pitch: 0, yaw: 0, roll: 0 },
        scale: { x: 1, y: 1, z: 1 },
    });
}

export function tick(deltaSeconds) {
    // Move or rotate the actor.
}

export function dispose() {
    if (actor) {
        ue.destroy(actor);
        actor = 0;
    }
}
```

The runtime loads JavaScript directly. There is no TypeScript or bundling step in the current workflow.

## Reloading Scripts

The plugin registers this console command:

```text
RinRinJs.Reload
```

You can run it from:

- the in-game console, usually opened with `~`;
- the Unreal Editor Output Log command input.

Reload flow:

1. Call `dispose()` on the current main module when present.
2. Destroy actors registered through the JS actor handle registry.
3. Rebuild the V8 execution context.
4. Re-read the manifest and source files.
5. Load and evaluate the main module.
6. Call `start(context)` again.

This makes it possible to edit `Content/Mods/Core/main.js`, run `RinRinJs.Reload`, and see behavior changes without restarting the editor.

## Debugging With Chrome DevTools

The plugin starts a V8 Inspector transport when the V8 context is created.

Default local WebSocket endpoint:

```text
ws://127.0.0.1:9229/
```

Recommended direct DevTools URL:

```text
devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/
```

Discovery endpoints:

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

Recommended workflow:

1. Run the Unreal project.
2. Open Chrome or Edge.
3. Open `devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/`.
4. Attach DevTools directly to the local V8 target.

The Inspector transport is local-only by default and uses CivetWeb for HTTP/WebSocket handling.

`chrome://inspect` still works as a discovery entry point, but the separate popup window opened from there may not reflect disconnect/reload state as clearly as the direct `devtools://...` URL.

## Project Map

For source layout, runtime layers, and dependency flow, see:

- English: [docs/project-map.md](docs/project-map.md)
- Simplified Chinese: [docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- Japanese: [docs/project-map.ja.md](docs/project-map.ja.md)

## Design Notes

The implementation keeps several boundaries visible:

- `FRinRinJsModule` is the Unreal-facing plugin entry point.
- V8-specific code stays in `Private/V8`.
- Script package lifecycle lives in `Private/Runtime`.
- JS-to-UE calls are whitelisted in `Private/Bridge`.
- Script file resolution is clamped to the package root.
- Runtime errors are returned through `TExpected` and `FError` where practical.
- Actor references exposed to JS are opaque integer handles, not raw UObject pointers.

## Future Work

Likely next steps:

- Add a small debug UI for common runtime commands such as reload.
- Turn the script package loader into a package/mod registry with explicit load order.
- Define ownership policy for JS-created actors versus shared/global actors.
- Expand the bridge beyond `AStaticMeshActor` while keeping native APIs explicit and permissioned.
- Add typed C++/JS value conversion for arrays, objects, and Unreal references.
- Add UObject/UFunction binding behind a deliberate allowlist.
- Improve reload behavior around DevTools sessions and state inspection.
- Add automated Windows build/smoke-test notes.
- Decide whether legacy ChakraCore code should remain as history or be removed from the active plugin.
