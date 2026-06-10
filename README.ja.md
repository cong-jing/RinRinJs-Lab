# RinRinJs-Lab / RinRinJs

## Maintenance Notice

このリポジトリは主にポートフォリオおよび採用選考向けの公開物として置いています。現時点では継続的な保守、機能追加の約束、外部サポートは提供できません。

`RinRinJs-Lab` は Unreal Engine 5.7 のサンプルプロジェクトです。中心となる `RinRinJs` プラグインは Google V8 を Unreal Engine に組み込み、プロジェクト内の JavaScript ES Module パッケージを読み込み、制御された Unreal API を JavaScript へ公開し、ゲームループからスクリプトを tick します。

このリポジトリは主にポートフォリオおよびソースコード展示用です。ネイティブエンジン統合、V8 組み込み、Unreal モジュール境界、構造化エラー処理、Chrome/Edge DevTools によるデバッグ、JavaScript 駆動の gameplay loop を示します。

デモ動画: JavaScript の編集、Unreal の実行画面、ホットリロード、そして Chrome DevTools によるデバッグ。

https://github.com/user-attachments/assets/be83b3a8-c648-420e-9dd5-5a582635e4c4

Language versions:

- English: [README.md](README.md)
- 简体中文: [README.zh-CN.md](README.zh-CN.md)
- 日本語: `README.ja.md`

## Current Status

実装済み:

- Unreal runtime plugin module `RinRinJs`
- V8 process / isolate / context lifecycle
- ES Module loading and dependency resolution
- Script package loading through `rinrin.manifest.json`
- Script lifecycle: `start(context)`, `tick(deltaSeconds)`, `dispose()`
- `URinRinJsLabGameInstance` integration
- JavaScript global bridge `ue`
- Actor creation/control through opaque actor handles
- Runtime reload through `RinRinJs.Reload`
- V8 Inspector transport for Chrome/Edge DevTools
- Structured errors through `TExpected` and `FError`

The current bridge exposes:

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

## Platform

Current target:

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- V8 monolithic static library

V8 headers and the Win64 release library are expected at:

```text
Plugins/RinRinJs/ThirdParty/v8
```

## Script Package

The current package lives at:

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

`main.js` exports lifecycle functions:

```js
export function start(context) {}
export function tick(deltaSeconds) {}
export function dispose() {}
```

The runtime loads JavaScript source files directly. There is no TypeScript or bundling step in the current workflow.

## Reload

Run this Unreal console command to reload the active script package:

```text
RinRinJs.Reload
```

It calls `dispose()`, destroys JS-spawned actors, rebuilds the V8 execution context, reloads the manifest and source files, and calls `start(context)` again.

## Debugging

V8 Inspector is exposed locally:

```text
ws://127.0.0.1:9229/
http://127.0.0.1:9229/json/list
```

Recommended direct DevTools URL:

```text
devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/
```

Open the direct `devtools://...` URL in Chrome or Edge and attach to the local V8 target.

`chrome://inspect` can still be used as a discovery entry point, but the separate popup window opened from there may not reflect disconnect/reload state as clearly as the direct `devtools://...` URL.

## Project Map

For source layout and dependency flow, see:

- English: [docs/project-map.md](docs/project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](docs/project-map.ja.md)

## Future Work

- Add a small debug UI for runtime commands.
- Expand script package loading into a package/mod registry.
- Define actor ownership policy for JS-created and shared actors.
- Expand the native bridge through explicit allowlists.
- Add typed C++/JS value conversion and UObject/UFunction binding.
- Improve reload and DevTools session behavior.
