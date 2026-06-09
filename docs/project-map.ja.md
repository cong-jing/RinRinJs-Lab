# Project Map

この文書では、`RinRinJs-Lab` と `RinRinJs` プラグインの主なソース構成、レイヤー分割、依存関係の流れを説明します。

言語版:

- English: [docs/project-map.md](project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](project-map.zh-CN.md)
- 日本語: `docs/project-map.ja.md`

## トップレベル構成

```text
RinRinJsLab.uproject
Source/RinRinJsLab
Content/Mods/Core
Plugins/RinRinJs
```

役割:

- `RinRinJsLab.uproject` は Unreal プロジェクト本体を定義する
- `Source/RinRinJsLab` はサンプルゲームモジュールを含む
- `Content/Mods/Core` はランタイムが読み込む JavaScript サンプルを含む
- `Plugins/RinRinJs` は V8 を組み込む Unreal プラグイン本体

## ゲームモジュール

```text
Source/RinRinJsLab/
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
  RinRinJsLab.Build.cs
```

役割:

- サンプルの起動/終了フローを持つ
- `FRinRinJsModule` を呼び出す
- モジュールパス解決を提供する
- JavaScript ソース読み込みを提供する
- ゲームコードがプラグインへ接続する方法を示す

現在の依存:

```text
RinRinJsLab.Build.cs
  -> PrivateDependencyModuleNames
    -> RinRinJs
```

## プラグイン記述とビルド

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

役割:

- `RinRinJs` を Runtime プラグインモジュールとして宣言する
- `Core`、`CoreUObject`、`Projects` などの Unreal 依存を追加する
- V8 の include と library パスを追加する
- 現在の Win64 静的 V8 ビルドに合わせたコンパイル定義を追加する
- CivetWeb の WebSocket サポート用コンパイル定義を追加する

## プラグイン公開 API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
  Value/
```

役割:

- `RinRinJs.h`: Unreal 側から使う主 API `FRinRinJsModule` を宣言
- `ModuleResolver.h`: ホスト側が提供する解決/読み込み callback 型を宣言
- `Value/ValueIntoJs.h`: C++ から JS に渡す値の型を定義
- `Value/ValueFromJs.h`: JS から C++ に返る値をラップ
- `Util/Expected.h`: 成功またはエラーを表す返り値を定義
- `Util/Error.h`: ソース位置や任意のスタック情報を持つ構造化エラーを定義
- `Util/Log.h`: プラグイン用ログ補助を定義

重要な境界:

- Public ヘッダからはできるだけ V8 型を漏らさない
- V8 固有コードは `Private` に閉じ込め、プラグイン側ラッパー越しに扱う

## プラグインモジュール実装

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

役割:

- `FRinRinJsModule` を実装する
- モジュール起動時にプロセスレベルの V8 状態を初期化する
- 明示的な start/stop に応じて実行時状態を生成・破棄する
- ゲームコードへモジュール読み込みとスクリプト評価 API を提供する

## V8 Runtime レイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Runtime.h
  V8Runtime.cpp
  V8EsModuleLoader.h
  V8EsModuleLoader.cpp
  V8Includes.h
```

役割:

- V8 platform 初期化を管理する
- V8 isolate/context のライフサイクルを管理する
- 直接渡された JavaScript 文字列を評価する
- ES Modules を読み込み実行する
- コンパイル済みモジュールをキャッシュする
- V8 の値をエンジン向けラッパー型へ変換する

概念的な流れ:

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

役割:

- ホスト callback を通じてモジュール specifier を解決する
- ホスト callback を通じて JavaScript ソースを読み込む
- `v8::ScriptCompiler::CompileModule` でモジュールをコンパイルする
- `v8::Module::InstantiateModule` でモジュールを instantiate する
- `v8::Module::Evaluate` でモジュールを実行する
- resolved id 単位でモジュールをキャッシュする
- 相対 import 解決のため referrer module id を追跡する

ホスト callback:

```text
FResolveModuleIdFn
  (referrer id, import specifier) -> resolved module id

FLoadSourceByModuleIdFn
  (resolved module id) -> UTF-8 source
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

- ローカル WebSocket 経由で Chrome DevTools Protocol を公開する
- `/json`、`/json/list`、`/json/version` discovery endpoint を提供する
- V8 Inspector session を生成・管理する
- DevTools メッセージを V8 に橋渡しする
- JS console 出力を Unreal ログへ流す
- Unreal ticker から transport のメッセージ処理を回す

依存の流れ:

```text
FV8Runtime
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## Value と Utility レイヤー

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Value/
Plugins/RinRinJs/Source/RinRinJs/Private/Value/
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

役割:

- V8 の値と公開値ラッパーの相互変換
- V8 handle や context の詳細を private 実装に閉じ込める
- `TExpected` を通じて失敗を返す
- `FError` に構造化エラー情報を保持する
- ソース位置付きでログを整形する

現在の値型:

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

## Package Registry 作業領域

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  PackageLoader.h
  PackageRegistry.h
  PackageRegistry.cpp
```

現在の意図:

- package manifest を発見する
- package メタデータをキャッシュする
- load order に従って package を並べる
- 実際のロード処理は loader interface に委譲する

この領域はまだ探索段階であり、安定したランタイム機能ではありません。

## サードパーティ依存

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/ThirdParty/ChakraCore
```

役割:

- V8 は JavaScript エンジンと Inspector API を提供する
- CivetWeb は DevTools 用のローカル HTTP/WebSocket transport を提供する
- ChakraCore は初期探索や代替案の履歴として残している

## 全体の依存フロー

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
