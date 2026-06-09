# RinRinJs-Lab / RinRinJs

`RinRinJs-Lab` は Unreal Engine 5.7 ベースのサンプルプロジェクトであり、その中心となるのが `RinRinJs` というランタイムプラグインです。このプラグインは Google V8 を Unreal Engine に組み込み、C++ のゲームコードから JavaScript の読み込み、実行、デバッグを行えるようにします。

このリポジトリは主にポートフォリオおよびソースコード展示用です。ネイティブエンジン統合、サードパーティランタイムの埋め込み、Unreal のモジュール境界設計、エラーハンドリング、開発者向けツール統合に対する自分の実装方針を示すことを目的としています。探索的なプロジェクトであり、製品として継続保守するプラグインではありません。そのため、互換性保証や外部利用者向けの長期サポートは提供していません。

言語版:

- English: [README.md](README.md)
- 简体中文: [README.zh-CN.md](README.zh-CN.md)
- 日本語: `README.ja.md`

## プロジェクトの目的

このプロジェクトの目的は、Unreal Engine 向けの JavaScript ランタイム層を検証し、将来的に次のような機能につなげることです。

- C++ から JavaScript を使ったゲームロジックスクリプト化
- Mod やユーザー作成スクリプト向けワークフロー
- プロジェクトコンテンツからの ES Module 読み込み
- V8 Inspector / Chrome DevTools によるブラウザベースのデバッグ
- ログ出力だけに依存しない、構造化された C++ エラー伝播
- JavaScript と Unreal のオブジェクト、関数、ゲームシステム連携の土台づくり

現在は最も難しい部分を先に扱っています。つまり、Windows + MSVC 環境で V8 を UE の Runtime モジュールへ安定して埋め込むことです。V8 はこの環境ではビルド設定、CRT/リンク整合性、Unreal Build Tool との相互作用、プラットフォームやコンパイラのマクロ整合など、統合難度が高いエンジンです。そのため、現時点での対応ランタイムプラットフォームは Windows のみです。

## 現在の実装状況

実装済み:

- Unreal ランタイムプラグイン `RinRinJs`
- V8 のプロセスレベル初期化と終了処理
- V8 isolate/context の生成と破棄
- C++ からの直接 JavaScript 実行
- ES Module の読み込みと依存解決
- `Content/Mods/Core` 配下のサンプルスクリプト
- Chrome/Edge DevTools で利用できる WebSocket ベースの V8 Inspector
- `/json/list` などのローカル Inspector discovery endpoint
- C++ と JS 間の値ラッパー
- `TExpected` と `FError` による構造化された結果/エラーモデル
- 最小構成の `GameInstance` 統合サンプル

探索中または未完成:

- package/mod registry と manifest 読み込み
- 型付き引数で JS の export 関数を呼ぶ安定した公開 API
- UObject / UFunction バインディング
- promise / async と UE tick または latent action の統合
- クロスプラットフォーム向け V8 ビルド
- 実プロジェクト向けのパッケージング/配布フロー

現時点で目標にしていないもの:

- Marketplace 向け完成形プラグイン
- 後方互換を保証する公開 API
- マルチプラットフォーム向けバイナリ配布
- Node.js 互換や大規模な JavaScript 実行環境の再現

## 対応プラットフォーム

現在の対象:

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- V8 は monolithic static library としてリンク

リポジトリには V8 のヘッダと Windows Release 用静的ライブラリが含まれています。

```text
Plugins/RinRinJs/ThirdParty/v8
```

現在の V8 は MSVC/Win64 向けの単一静的ライブラリ構成でビルドしています。主要なビルド設定は次の通りです。

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

## 開き方とビルド

通常、このプロジェクトは標準的な Unreal Engine プロジェクトとして扱うのが自然です。ビルドは特定の IDE に依存するものではなく、Unreal Engine と Unreal Build Tool によって行われます。

推奨環境:

- Unreal Engine 5.7
- C++ デスクトップ開発ツールチェーンを含む Visual Studio 2022
- 現在の UE ツールチェーンと互換性のある Windows SDK

clone 後の一般的な初回手順:

1. `Plugins/RinRinJs/ThirdParty/v8` にリポジトリ同梱の V8 ヘッダと Win64 静的ライブラリが存在することを確認する。
2. Unreal Engine 5.7 で `RinRinJsLab.uproject` を開く。
3. Unreal からプロジェクトファイル生成を求められたら実行する。
4. Unreal Editor からプロジェクトをビルドするか、初回起動時に Unreal が自動で行うコンパイルに任せる。

補足:

- 日常的なコンパイルは Unreal Editor から直接行えます。
- Visual Studio や VSCode は、同じ UBT ベースのビルドパイプラインを扱うための任意の開発環境です。
- VSCode 用の workspace や IntelliSense、task が必要な場合にのみ、VSCode 向けのプロジェクト生成が必要になります。

## 実行の流れ

サンプルプロジェクトでは `URinRinJsLabGameInstance` から JavaScript ランタイムを起動しています。

起動時:

1. Unreal が `URinRinJsLabGameInstance::Init()` を呼びます。
2. `FModuleManager` 経由で `RinRinJs` モジュールを取得します。
3. `FRinRinJsModule::StartRuntime()` を呼びます。
4. プラグインが V8 を初期化し、実行コンテキストを生成します。
5. ゲーム側が `"main"` JavaScript モジュールを読み込みます。
6. 現在は `foo(2, 3)` というテスト式を評価します。

終了時:

1. `URinRinJsLabGameInstance::Shutdown()` で再度プラグインモジュールを取得します。
2. `FRinRinJsModule::StopRuntime()` を呼びます。
3. プラグインがロード済みモジュール、Inspector、V8 context、isolate、allocator、およびプロセスレベルの V8 リソースを解放します。

現在のサンプルでは、エントリモジュールは次のように解決されます。

```text
main -> Content/Mods/Core/main.js
```

相対 ESM import は、インポート元モジュールのディレクトリを基準に解決されます。

## 基本的な使い方

現在の C++ 側公開エントリポイントは `FRinRinJsModule` です。

サンプル `GameInstance` での呼び出し例:

```cpp
if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
{
    FRinRinJsModule& Module =
        FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");

    Module.StartRuntime();

    auto LoadResult = Module.LoadJsModule(
        "main",
        &URinRinJsLabGameInstance::resolveModulePath,
        &URinRinJsLabGameInstance::LoadJavascriptFile);

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

ホストゲーム側は現在 2 つの callback を提供します。

- `FResolveModuleIdFn`: import specifier をモジュール id に解決する。現在は正規化されたファイルパス
- `FLoadSourceByModuleIdFn`: モジュール id に対応する UTF-8 JavaScript ソースを読み込む

この構成により、ファイルシステムポリシーを V8 層に固定せず、ホスト側に残せます。将来的に pak、ダウンロードコンテンツ、仮想ファイルシステム、エディタ管理アセットなどへ拡張しやすくなっています。

## JavaScript サンプル

現在のサンプルスクリプトは次にあります。

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

`globalThis.foo = foo` を残しているのは、現在の直接評価パスでは ESM 実行後にグローバル関数を呼び出す形を使っているためです。より望ましい今後の方向としては、C++ からモジュール export を型付き API で直接呼び出す形を考えています。

## Chrome DevTools でのデバッグ

V8 context が生成されると、プラグインは V8 Inspector transport も起動します。

デフォルトのローカル endpoint:

```text
ws://127.0.0.1:9229/
```

discovery endpoint:

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

基本的なデバッグ手順:

1. Windows 上で Unreal プロジェクトを実行する
2. Chrome または Edge を開く
3. `chrome://inspect` にアクセスする
4. `127.0.0.1:9229` を確認または設定する
5. 公開されている V8 target に接続する

Inspector transport はデフォルトでローカルアクセスのみに制限されています。HTTP/WebSocket には CivetWeb を使用し、プロトコルメッセージは Unreal の ticker からポンプされます。

## プロジェクト構成

より詳しいソースツリー、レイヤー構成、依存関係の流れは次を参照してください。

- English: [docs/project-map.md](docs/project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](docs/project-map.ja.md)

## 設計メモ

現在の実装では、いくつかの境界を意図的に明確にしています。

- Unreal 向け API は `Public` に置く
- V8 固有型はできるだけ `Private` に閉じ込める
- モジュール解決とソース読み込み方針はホストゲームに持たせる
- 実行時エラーはログだけでなく `TExpected` で返す
- ブラウザデバッグを後付けではなくランタイム体験の一部として扱う

ポートフォリオ用プロジェクトとして、この形は最終機能だけでなく、複雑なネイティブ依存を大規模エンジンへ組み込む際の設計判断も見せられる構成になっています。

## 今後の予定

今後進める可能性が高い項目:

- package/mod registry と manifest 設計の完成
- 安定した `ExecuteJsFunction` API を `FRinRinJsModule` に公開
- グローバル関数依存ではなく ES Module export を直接呼び出す形へ移行
- `FValueIntoJs` / `FValueFromJs` を拡張し、配列、オブジェクト、Unreal 参照に対応
- UObject / UFunction バインディング追加
- promise / microtask と Unreal tick の整合
- TypeScript と source map デバッグの改善
- 古い ChakraCore 関連コードの整理、または履歴資料化
- Windows 向けビルド手順と smoke test 文書の追加
