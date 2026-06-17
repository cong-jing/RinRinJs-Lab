# RinRinJs-Lab / RinRinJs

言語版:

- English: [README.md](README.md)
- 简体中文: [README.zh-CN.md](README.zh-CN.md)
- 日本語: `README.ja.md`

## メンテナンスに関するお知らせ

このリポジトリは主にポートフォリオおよび採用選考向けの公開物として置いています。現時点では、継続的な保守、機能追加の約束、外部サポートは提供できません。

`RinRinJs-Lab` は Unreal Engine 5.7 のサンプルプロジェクトです。中心となる `RinRinJs` は、Google V8 を Unreal Engine に組み込むランタイムプラグインです。このプロジェクトは、プロジェクトコンテンツ内の ES Module スクリプトパッケージを読み込み、小さな Unreal 向け JavaScript ブリッジを公開し、ゲームループからスクリプトのライフサイクル関数を駆動し、Chrome/Edge DevTools で JavaScript をデバッグできます。

このリポジトリは主にポートフォリオおよびソースコード展示用です。ネイティブエンジン統合、サードパーティランタイムの組み込み、Unreal モジュール境界、構造化エラー処理、ブラウザによるデバッグ、スクリプト駆動の gameplay loop を示します。

デモ動画: JavaScript の編集、Unreal ランタイムでの再生、ホットリロード、Chrome DevTools によるデバッグ。

https://github.com/user-attachments/assets/be83b3a8-c648-420e-9dd5-5a582635e4c4

## 現在の状態

実装済み:

- Unreal ランタイムプラグインモジュール `RinRinJs`。
- V8 プロセスの初期化と終了処理。
- V8 isolate/context の作成とクリーンアップ。
- C++ からの JavaScript ソースの直接評価。
- 依存解決とモジュールキャッシュ処理を含む ES Module 読み込み。
- `rinrin.manifest.json` によるスクリプトパッケージ読み込み。
- スクリプトライフサイクル: `start(context)`, `tick(deltaSeconds)`, `dispose()`。
- ランタイムを開始し、ゲームワールドの begin play を待ち、スクリプトパッケージを読み込み、毎フレーム JS を tick する `URinRinJsLabGameInstance` 統合。
- JavaScript に注入される小さなネイティブ `ue` ブリッジ。
- 不透明なアクターハンドルによる Actor の生成と制御。
- `RinRinJs.Reload` コンソールコマンドによるランタイムリロード。
- Chrome/Edge DevTools 向け WebSocket 経由の V8 Inspector 統合。
- `/json/list` などのローカル Inspector ディスカバリエンドポイント。
- `TExpected` と `FError` による構造化された error/result プリミティブ。

現在の JavaScript ブリッジ:

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

現時点では提供していないもの:

- 安定した公開 API 互換性の保証。
- Marketplace 対応のプラグインパッケージング。
- Node.js 互換性。
- クロスプラットフォームの V8 バイナリ。
- 汎用的な UObject/UFunction リフレクション。
- 完全な package/mod マネージャー。

## プラットフォームサポート

現在のターゲット:

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- モノリシック静的ライブラリとしてリンクされる V8

このリポジトリでは、V8 ヘッダーと Win64 release 静的ライブラリが次の場所にあることを想定しています:

```text
Plugins/RinRinJs/ThirdParty/v8
```

## 開いてビルドする

推奨セットアップ:

- Unreal Engine 5.7
- C++ デスクトップツールチェーンを含む Visual Studio 2022
- インストール済み UE ツールチェーンと互換性のある Windows SDK

初回の一般的な流れ:

1. `Plugins/RinRinJs/ThirdParty/v8` に同梱 V8 ヘッダーと Win64 ライブラリが含まれていることを確認します。
2. Unreal Engine 5.7 で `RinRinJsLab.uproject` を開きます。
3. 確認を求められた場合は、Unreal にプロジェクトファイルを生成させます。
4. Unreal Editor、Visual Studio、または Unreal Build Tool からビルドします。

## ランタイムフロー

ゲームモジュールは `URinRinJsLabGameInstance` からランタイムを開始し、駆動します。

起動フロー:

1. `URinRinJsLabGameInstance::Init()` が `RinRinJs` モジュールを取得します。
2. ゲーム側が `FRinRinJsModule::StartRuntime()` を呼び出します。
3. プラグインが V8 を初期化し、実行コンテキストを作成します。
4. ゲーム ticker がプラグインの world ポインタを更新します。
5. world が game world であり、`HasBegunPlay()` が true になると、ゲームは `Content/Mods/Core` を読み込みます。
6. `FScriptHost` が `rinrin.manifest.json` を読み、manifest の `main` モジュールを読み込み、`globalThis.ue` を注入し、`start({ packageName })` を呼び出します。
7. 各 tick で、存在する場合は export された `tick(deltaSeconds)` を呼び出します。

終了フロー:

1. `URinRinJsLabGameInstance::Shutdown()` が ticker を削除します。
2. ゲーム側がアクティブなスクリプトパッケージをアンロードします。
3. `FScriptHost` が export されている場合は `dispose()` を呼び出し、JS が spawn した actor を破棄し、actor handle をクリアします。
4. プラグインがランタイムを停止し、V8 context/process 状態を破棄します。

## スクリプトパッケージ

現在のスクリプトパッケージは次の場所にあります:

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

`main.js` はライフサイクル関数を export します:

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

ランタイムは JavaScript を直接読み込みます。現在のワークフローには TypeScript や bundling の手順はありません。

## スクリプトのリロード

プラグインは次のコンソールコマンドを登録します:

```text
RinRinJs.Reload
```

このコマンドは次の場所から実行できます:

- 通常 `~` で開くゲーム内コンソール。
- Unreal Editor の Output Log コマンド入力欄。

リロードフロー:

1. 現在の main module に `dispose()` が存在する場合は呼び出します。
2. JS actor handle registry に登録された actor を破棄します。
3. V8 実行コンテキストを再構築します。
4. manifest とソースファイルを再読み込みします。
5. main module を読み込み、評価します。
6. `start(context)` を再度呼び出します。

これにより、editor を再起動せずに `Content/Mods/Core/main.js` を編集し、`RinRinJs.Reload` を実行して、挙動の変化を確認できます。

## Chrome DevTools によるデバッグ

プラグインは V8 context が作成されたときに V8 Inspector transport を開始します。

既定のローカル WebSocket エンドポイント:

```text
ws://127.0.0.1:9229/
```

推奨される直接 DevTools URL:

```text
devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/
```

ディスカバリエンドポイント:

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

推奨ワークフロー:

1. Unreal プロジェクトを実行します。
2. Chrome または Edge を開きます。
3. `devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/` を開きます。
4. DevTools をローカル V8 target に直接 attach します。

Inspector transport は既定で local-only であり、HTTP/WebSocket 処理に CivetWeb を使います。

`chrome://inspect` もディスカバリの入り口として引き続き使えますが、そこから開かれる別 popup window は、直接 `devtools://...` URL を開いた場合ほど disconnect/reload 状態を明確に反映しないことがあります。

## プロジェクトマップ

ソースレイアウト、ランタイムレイヤー、依存関係の流れについては、次を参照してください:

- English: [docs/project-map.md](docs/project-map.md)
- 简体中文: [docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](docs/project-map.ja.md)

## 設計メモ

この実装では、いくつかの境界を明示しています:

- `FRinRinJsModule` は Unreal 向けのプラグイン entry point です。
- V8 固有のコードは `Private/V8` に置かれます。
- スクリプトパッケージのライフサイクルは `Private/Runtime` にあります。
- JS から UE への呼び出しは `Private/Bridge` で allowlist 化されます。
- スクリプトファイル解決は package root 内に制限されます。
- ランタイムエラーは、実用的な範囲で `TExpected` と `FError` を通して返されます。
- JS に公開される actor 参照は raw UObject pointer ではなく、不透明な integer handle です。

## 今後の作業

想定される次のステップ:

- reload などの一般的なランタイムコマンド向けに、小さな debug UI を追加する。
- スクリプトパッケージローダーを、明示的な load order を持つ package/mod registry に発展させる。
- JS が作成した actor と共有/global actor の所有ポリシーを定義する。
- ネイティブ API を明示的かつ permissioned に保ちながら、ブリッジを `AStaticMeshActor` 以外へ拡張する。
- 配列、オブジェクト、Unreal 参照向けの型付き C++/JS 値変換を追加する。
- 意図的な allowlist の背後に UObject/UFunction binding を追加する。
- DevTools session と state inspection まわりの reload 挙動を改善する。
- 自動化された Windows build/smoke-test メモを追加する。
- 旧 ChakraCore コードを履歴として残すか、アクティブなプラグインから削除するかを決める。
