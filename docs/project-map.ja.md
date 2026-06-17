# プロジェクトマップ

このドキュメントでは、`RinRinJs-Lab` と `RinRinJs` プラグインの現在のソースレイアウトと依存関係の流れを説明します。

言語版:

- English: [docs/project-map.md](project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](project-map.zh-CN.md)
- 日本語: `docs/project-map.ja.md`

## トップレベル

```text
RinRinJsLab.uproject
Config/
Source/RinRinJsLab/
Content/Mods/Core/
Plugins/RinRinJs/
```

役割:

- `RinRinJsLab.uproject` は Unreal プロジェクトを定義します。
- `Config/DefaultEngine.ini` はサンプルマップと `URinRinJsLabGameInstance` を設定します。
- `Source/RinRinJsLab` はホストゲームモジュールを含みます。
- `Content/Mods/Core` はランタイムが読み込むスクリプトパッケージを含みます。
- `Plugins/RinRinJs` は V8 を組み込むランタイムプラグインを含みます。

## ホストゲームモジュール

```text
Source/RinRinJsLab/
  RinRinJsLab.Build.cs
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
```

役割:

- `RinRinJs` プラグインモジュールに依存します。
- プラグインランタイムを開始および停止します。
- プラグインの world ポインタを更新し、script tick を駆動する ticker を登録します。
- スクリプトパッケージを読み込む前に、ゲームワールドが begin play 済みになるまで待ちます。
- `Content/Mods/Core` をアクティブなパッケージとして読み込みます。

現在のフロー:

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

## スクリプトパッケージ

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

役割:

- `rinrin.manifest.json` はパッケージメタデータと main module を定義します。
- `package.json` は editor/tooling サポートのために、このディレクトリを ESM として示します。
- `main.js` はライフサイクル関数を export します。

Manifest:

```json
{
    "name": "core-demo",
    "version": "0.1.0",
    "main": "main.js"
}
```

ライフサイクル export:

```text
start(context)
tick(deltaSeconds)
dispose()
```

## プラグイン記述子とビルド

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

役割:

- `RinRinJs` をランタイムプラグインモジュールとして宣言します。
- `Core`, `CoreUObject`, `Engine`, `Json`, `Projects` などの Unreal 依存関係を追加します。
- V8 の include/library パスを追加します。
- 同梱 Win64 静的ビルドに合わせた V8 compile macro を定義します。
- ローカル Inspector HTTP/WebSocket transport のために CivetWeb 定義を追加します。

## プラグイン公開 API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
    Error.h
    Expected.h
    Log.h
```

役割:

- `RinRinJs.h` は Unreal 向けプラグイン entry point である `FRinRinJsModule` を宣言します。
- `ModuleResolver.h` はホストが提供する module resolve/load callback 型を宣言します。
- `Util/Expected.h` は成功またはエラーを返す値を定義します。
- `Util/Error.h` は source location と任意の JS stack 情報を持つ構造化エラーを定義します。
- `Util/Log.h` はプラグインの logging helper を定義します。

重要な境界:

- 公開ヘッダーは V8 型を公開しません。
- ランタイムとブリッジの実装詳細は `Private` に置かれます。

## プラグインモジュール

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

役割:

- `FRinRinJsModule` を実装します。
- モジュール起動時に process-level の V8 状態を初期化します。
- 明示的なランタイム呼び出しを通して、実行コンテキスト状態を開始および停止します。
- スクリプトパッケージのライフサイクルをホストゲームへ公開します。
- `RinRinJs.Reload` コンソールコマンドを登録および解除します。

ホストが使用する公開モジュール呼び出し:

```text
StartRuntime()
StopRuntime()
SetGameWorld(UWorld*)
LoadScriptPackage(packageRoot)
UnloadScriptPackage()
ReloadScriptPackage()
TickRuntime(deltaSeconds)
```

## ランタイムレイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/
  ScriptHost.h
  ScriptHost.cpp
  ScriptManifest.h
  ScriptManifest.cpp
```

役割:

- `rinrin.manifest.json` を読み込みます。
- package root と main module のパスを正規化します。
- module resolution を package root 内に制限します。
- パッケージごとの native bridge と actor registry を作成します。
- V8 context に `globalThis.ue` を注入します。
- main ES module を読み込み、評価します。
- export されている場合は `start(context)`, `tick(deltaSeconds)`, `dispose()` を呼び出します。
- reload 時に V8 実行コンテキストを再構築します。
- unload/reload 時に JS が作成した actor を破棄します。

フロー:

```text
FRinRinJsModule
  -> FScriptHost
    -> LoadScriptManifest
    -> FNativeBridge
    -> FActorHandleRegistry
    -> FV8Loader
    -> FV8ModuleManager
```

## ブリッジレイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/
  NativeBridge.h
  NativeBridge.cpp
  ActorHandleRegistry.h
  ActorHandleRegistry.cpp
```

役割:

- JavaScript に `ue` object を注入します。
- Unreal API を呼び出す前に JS 引数を検証します。
- 単純な JS object を `FVector`, `FRotator`, `FTransform` に変換します。
- `UStaticMesh` asset path から `AStaticMeshActor` を spawn します。
- Outliner で見つけやすいように、spawn した actor に `RinRinJsDemoActor` prefix を付けます。
- actor 参照を不透明な integer handle の背後に保存します。
- 後続の bridge call のために handle を actor へ解決します。
- unload 時に登録済みの JS-created actor をすべて破棄します。

現在のブリッジ API:

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

## V8 レイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Loader.h
  V8Loader.cpp
  V8ModuleManager.h
  V8ModuleManager.cpp
  V8Console.h
  V8Console.cpp
```

役割:

- process-level の V8 resource を初期化および終了します。
- V8 platform、allocator、isolate、execution context を所有します。
- V8 context を作成および破棄します。
- `FV8ModuleManager` を作成して公開します。
- JavaScript source string を直接実行します。
- ホストの resolver/source-loader callback を通して ES Module を読み込みます。
- export された module function を実行します。
- V8 固有の型を公開プラグインヘッダーから隔離します。

概念上のフロー:

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

## Inspector レイヤー

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

役割:

- Chrome DevTools Protocol をローカル WebSocket 経由で公開します。
- `/json`, `/json/list`, `/json/version` ディスカバリエンドポイントを提供します。
- V8 Inspector session を作成および管理します。
- DevTools message を V8 へ橋渡しします。
- JS console message を Unreal log へ流します。
- Unreal の ticker から transport message を pump します。

依存関係の流れ:

```text
FV8Loader
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## ユーティリティレイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

役割:

- 失敗を `TExpected` で返します。
- `FError` に構造化された error context を保持します。
- 利用可能な場合は JS stack の詳細を取得して出力します。
- source location 情報を含む log を整形します。

## サードパーティ依存関係

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/Source/ChakraCoreLoader
```

役割:

- V8 は JavaScript engine と Inspector API を提供します。
- CivetWeb は DevTools 向けのローカル HTTP/WebSocket transport を提供します。
- ChakraCoreLoader は以前の runtime-loader 作業としてリポジトリに残っていますが、`RinRinJs_USE_V8=1` のときはアクティブな経路ではありません。

## 全体の依存関係の流れ

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

## 開発メモ

- `Content/Mods/Core/main.js` は、スクリプト挙動を反復するための最短の場所です。
- `RinRinJs.Reload` は現在のランタイムリロード entry point です。
- actor handle は意図的に不透明です。JavaScript が raw UObject pointer を受け取るべきではありません。
- スクリプト読み込みは、package root 外の import を引き続き拒否する必要があります。
- world に依存するスクリプト読み込みは、ゲームワールドが begin play 済みになった後に行う必要があります。
