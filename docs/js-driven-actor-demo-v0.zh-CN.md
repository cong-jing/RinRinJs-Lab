# JS-driven Actor Demo v0 实现计划

本文档记录 `RinRinJs-Lab` 下一阶段的最小可展示垂直切片实现计划。目标不是一次性完成完整 mod manager、UObject reflection 或 ScriptComponent 框架，而是在现有 V8 runtime 和 Inspector 基础上，做出一个可以录制视频、可以解释给招聘方看的 demo：

```text
UE 中运行 JavaScript
-> JS 调用受控 UE API
-> spawn actor
-> UE 每帧调用 JS tick
-> 修改 TypeScript 参数
-> tsc watch 编译
-> UE 中 reload script
-> actor 行为变化
-> Chrome DevTools 断点调试
-> JS 错误输出清晰 stack trace，UE 不崩溃
```

## 目标

- 加载一个 manifest 指定的 main JavaScript module。
- 调用 module-level lifecycle：`start(context)`、`tick(deltaTime)`、`dispose()`。
- 向 JS 注入一个受控的 `ue` API 对象。
- 通过 opaque actor handle 让 JS 控制 UE 场景中的 actor。
- 支持 UE 运行中执行 `RinRinJs.Reload` 重新加载脚本。
- 支持 TypeScript watch workflow：修改 TS 参数后，不重启 UE Editor 即可看到 actor 行为变化。
- 保持 Chrome DevTools / V8 Inspector 可用于调试 demo module。
- 保持 JS 错误安全：捕获错误、输出清晰信息和 stack trace，不让 UE 崩溃。

## 非目标

- 完整 package manager / mod dependency management。
- semver、lock file、remote package、mod conflict resolution。
- 自动 UObject reflection。
- 自动暴露 BlueprintCallable / UFunction。
- JS class extends Actor。
- 完整 ScriptComponent framework。
- per-actor script binding。
- full HMR 和状态保持。
- 复杂 async / promise 系统。
- 大规模重写当前 V8 runtime。

这些方向可以进入 roadmap，但不应该阻塞 demo v0。

## 当前代码现状

### 已具备能力

- `FRinRinJsModule` 是当前插件对外入口，位于：

```text
Plugins/RinRinJs/Source/RinRinJs/Public/RinRinJs.h
Plugins/RinRinJs/Source/RinRinJs/Private/RinRinJs.cpp
```

- V8 进程级初始化、isolate/context 生命周期由 `FV8Loader` 管理：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8Loader.h
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8Loader.cpp
```

- ES Module 加载、resolve、compile、evaluate、cache 由 `FV8ModuleManager` 管理：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8ModuleManager.h
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8ModuleManager.cpp
```

- V8 Inspector / Chrome DevTools transport 已经存在，并通过 `FTSTicker` pump Inspector 消息：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/Inspector/
```

- 当前 sample game 的 `URinRinJsLabGameInstance` 负责启动 runtime、加载 `main` module、提供文件 resolve/load callback：

```text
Source/RinRinJsLab/RinRinJsLabGameInstance.h
Source/RinRinJsLab/RinRinJsLabGameInstance.cpp
```

- 结构化错误和 JS stack 捕获已经有基础：

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Util/Expected.h
Plugins/RinRinJs/Source/RinRinJs/Public/Util/Error.h
Plugins/RinRinJs/Source/RinRinJs/Private/Util/Error.cpp
```

### 当前缺口

- 没有 native UE API 注入 JS context。
- 没有 ActorHandle registry。
- 没有 `start/tick/dispose` lifecycle manager。
- 没有运行时 reload command。
- 没有 manifest loader v0。
- 没有 TypeScript watch 目录和配置。
- 没有适合 demo 的 actor spawn API。
- 当前 `LoadJsModule` 里还有临时 `foo/bar` 测试调用，应迁移到 lifecycle 调用。
- 当前没有 Public/Private `Value` wrapper 实现；v0 不依赖完整值系统，先使用 V8 object/int/string 的局部转换。

## 推荐边界

v0 的核心原则是：临时代码可以存在，但必须集中在 host/bridge/demo 层，不污染 V8 runtime core。

建议结构：

```text
FRinRinJsModule
  -> FJsDemoRuntime / FScriptHost
    -> FScriptManifestLoader
    -> FV8Loader
      -> FV8ModuleManager
    -> FNativeBridge
    -> FActorHandleRegistry
```

未来演进到 mod manager 时，主要替换 `FScriptManifestLoader` 和 `FJsDemoRuntime` 的包组织逻辑，而不是推倒 V8、Inspector、错误模型等底座。

## 推荐新增文件

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptHost.h
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptHost.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptManifest.h
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptManifest.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/NativeBridge.h
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/NativeBridge.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/ActorHandleRegistry.h
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/ActorHandleRegistry.cpp
```

命名可以调整，但建议保留三层含义：

- `Runtime`：script lifecycle、reload、manifest。
- `Bridge`：JS 到 UE 的白名单 API。
- `ActorHandleRegistry`：opaque handle 到 UE actor 的映射。

## Manifest v0

建议新增：

```text
Content/Mods/Core/rinrin.manifest.json
```

字段：

```json
{
  "name": "core-demo",
  "version": "0.1.0",
  "main": "dist/main.js"
}
```

v0 不处理 dependencies、semver、load order、权限系统。manifest 的目的只是明确 script package 边界和入口。

## JS Lifecycle v0

main module 暴露：

```ts
export function start(context) {}
export function tick(deltaTime) {}
export function dispose() {}
```

调用规则：

- `start(context)`：首次加载或 reload 成功后调用。
- `tick(deltaTime)`：UE 每帧调用。
- `dispose()`：reload 或 shutdown 前调用。

`context` v0 可以很薄：

```js
{
  packageName: "core-demo"
}
```

实际 UE API 先通过 `globalThis.ue` 暴露，后续再决定是否全部移动到 `context.ue`。

## Native UE API v0

第一阶段只暴露白名单 API：

```ts
ue.log(message: string): void
ue.spawnActorByPath(assetPath: string, transform?: Transform): ActorHandle
ue.destroy(actor: ActorHandle): void
ue.setLocation(actor: ActorHandle, location: Vector): void
ue.getLocation(actor: ActorHandle): Vector
ue.setRotation(actor: ActorHandle, rotation: Rotator): void
ue.setTransform(actor: ActorHandle, transform: Transform): void
ue.addWorldOffset(actor: ActorHandle, offset: Vector): void
ue.setVisible(actor: ActorHandle, visible: boolean): void
```

`ActorHandle` v0 建议使用 number。C++ 侧使用递增 `int32` 作为 id，registry 内部保存 `TWeakObjectPtr<AActor>`。

## Transform 数据结构

v0 使用 plain object：

```ts
type Vector = { x: number; y: number; z: number };
type Rotator = { pitch: number; yaw: number; roll: number };
type Transform = {
  location?: Vector;
  rotation?: Rotator;
  scale?: Vector;
};
```

转换规则应尽量宽容：

- 缺失 `location` 时使用 `{ x: 0, y: 0, z: 0 }`。
- 缺失 `rotation` 时使用 `{ pitch: 0, yaw: 0, roll: 0 }`。
- 缺失 `scale` 时使用 `{ x: 1, y: 1, z: 1 }`。
- 字段类型错误时返回 JS exception，并通过 `FError` 输出清晰信息。

## ActorHandle Registry

职责：

- 生成 actor handle。
- 保存 `TWeakObjectPtr<AActor>`。
- 校验 handle 是否有效。
- 记录哪些 actor 由 JS 创建。
- reload/dispose/shutdown 时销毁 JS 创建的 actor。

建议行为：

- JS 调用无效 handle 时抛出 JS `Error`，不要让 C++ 崩溃。
- actor 已被 UE 侧销毁时，API 返回清晰错误。
- reload 出错时也要尽量清掉旧 actor，避免残留。

## Tick 驱动

v0 可以先由 `URinRinJsLabGameInstance` 注册 `FTSTicker` 来调用 `FRinRinJsModule::TickRuntime(float)`。

这样实现最快，也避免立刻引入 Subsystem。未来如果要收紧边界，可以迁移到：

- `UGameInstanceSubsystem`
- `UWorldSubsystem`
- plugin 内部 ticker + explicit world binding

v0 迁移路线：

```text
GameInstance ticker
  -> FRinRinJsModule::TickRuntime(deltaTime)
    -> FScriptHost::Tick(deltaTime)
      -> FV8ModuleManager::ExecuteFunction(mainModuleId, "tick", ...)
```

## Reload v0

注册 console command：

```text
RinRinJs.Reload
```

建议流程：

1. 暂停 tick 或设置 reload guard。
2. 调用当前 module 的 `dispose()`。
3. 销毁 JS 创建的 actor。
4. 清理 actor handles。
5. 清理 module cache 或重建 V8 execution context。
6. 重新读取 manifest。
7. 重新加载 main module。
8. 调用 `start(context)`。
9. 恢复 tick。

v0 可以选择“重建 execution context”的方式降低缓存和 stale reference 风险。这样不是 full HMR，但稳定、可解释，也更适合 demo。

## TypeScript Workflow

建议目录：

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  tsconfig.json
  src/
    demo.ts
  dist/
    main.js
```

`package.json` v0：

```json
{
  "type": "module",
  "scripts": {
    "watch": "tsc -w",
    "build": "tsc"
  },
  "devDependencies": {
    "typescript": "^5.0.0"
  }
}
```

Runtime 加载 `dist/main.js`，不要直接加载 `.ts`。

## Demo Script 行为

最小 demo：

```ts
const radius = 300;
const speed = 1.2;
const heightAmplitude = 80;
const rotationSpeed = 90;

let actor = 0;
let time = 0;

export function start() {
  actor = ue.spawnActorByPath("/Engine/BasicShapes/Cube.Cube", {
    location: { x: radius, y: 0, z: 120 },
    scale: { x: 1, y: 1, z: 1 }
  });
}

export function tick(deltaTime) {
  time += deltaTime;

  const angle = time * speed;
  ue.setLocation(actor, {
    x: Math.cos(angle) * radius,
    y: Math.sin(angle) * radius,
    z: 120 + Math.sin(angle * 2) * heightAmplitude
  });

  ue.setRotation(actor, {
    pitch: 0,
    yaw: time * rotationSpeed,
    roll: 0
  });
}

export function dispose() {
  if (actor) {
    ue.destroy(actor);
    actor = 0;
  }
}
```

视频展示时修改 `radius` 或 `speed`，保存后由 `tsc -w` 编译，再在 UE 中执行 `RinRinJs.Reload`。

## 分阶段实施计划

### 1. 修复 module export 调用底座

目标：

- C++ 能稳定调用任意 ESM export function。

涉及文件：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8ModuleManager.h
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8ModuleManager.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8Loader.cpp
```

内容：

- 让 `ExecuteFunction` 在 `kEvaluated` 状态下也能读取 module namespace。
- 移除或隔离 `LoadJsModule` 内部 `foo/bar` 临时测试调用。
- 增加更通用的 `CallModuleFunction` 接口。

风险：

- V8 module status 处理不当会导致 lifecycle 函数找不到或重复 evaluate。

验证：

- 现有 `Content/Mods/Core/main.js` 的 `foo/bar` 仍可被 C++ 调用。

### 2. 注入最小 native bridge

目标：

- JS 可以调用 `ue.log("message")`。

涉及文件：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/V8Loader.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/NativeBridge.*
```

内容：

- 在 V8 context 创建后注入 `globalThis.ue`。
- 先实现 `ue.log`。

风险：

- native callback 中参数校验不严会导致 V8 exception 不清晰。

验证：

- JS module 中调用 `ue.log(...)`，UE Output Log 能看到消息。

### 3. ActorHandle registry 和 actor API v0

目标：

- JS 能 spawn、移动、销毁 actor。

涉及文件：

```text
Plugins/RinRinJs/Source/RinRinJs/RinRinJs.Build.cs
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/ActorHandleRegistry.*
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/NativeBridge.*
```

内容：

- 给 plugin 增加 `Engine` 依赖。
- registry 使用 `TWeakObjectPtr<AActor>`。
- 实现 `spawnActorByPath`、`destroy`、`setLocation`、`getLocation`。

风险：

- `UWorld*` 生命周期和 reload 时机需要小心。
- asset path 加载失败时必须返回清晰错误。

验证：

- `start()` 中 spawn cube。
- `tick()` 中 setLocation，actor 可见移动。
- `dispose()` 中 actor 被销毁。

### 4. ScriptHost lifecycle

目标：

- 支持 `start/tick/dispose`。

涉及文件：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptHost.*
Plugins/RinRinJs/Source/RinRinJs/Public/RinRinJs.h
Plugins/RinRinJs/Source/RinRinJs/Private/RinRinJs.cpp
Source/RinRinJsLab/RinRinJsLabGameInstance.*
```

内容：

- `ScriptHost` 持有当前 main module id。
- 加载 module 后调用 `start(context)`。
- 每帧调用 `tick(deltaTime)`。
- shutdown 调用 `dispose()`。

风险：

- JS tick 抛错后是否继续 tick，需要定义策略。v0 建议本帧记录错误，后续 tick 可以继续；连续错误可后续再加保护。

验证：

- actor 绕圈移动。
- 注释掉 `tick` 时系统不崩溃，只记录缺失或跳过。

### 5. Reload command

目标：

- 支持 `RinRinJs.Reload`。

涉及文件：

```text
Plugins/RinRinJs/Source/RinRinJs/Private/RinRinJs.cpp
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/ScriptHost.*
```

内容：

- 注册 `FAutoConsoleCommand` 或 `IConsoleManager` command。
- reload 时执行 dispose、清 actor、重读 manifest、重建/重载 context、start。

风险：

- DevTools session 在重建 context 后可能需要重新连接。
- reload 中途失败要保持 plugin 可再次 reload。

验证：

- 修改 JS 后执行 `RinRinJs.Reload`，actor 行为变化。
- 故意写错 JS，reload 输出错误；修正后 reload 可恢复。

### 6. TypeScript demo workflow

目标：

- 支持 TS 参数修改和 watch 编译。

涉及文件：

```text
Content/Mods/Core/package.json
Content/Mods/Core/tsconfig.json
Content/Mods/Core/src/demo.ts
Content/Mods/Core/dist/main.js
```

内容：

- 配置 `tsc` 编译到 `dist`。
- manifest 指向 `dist/main.js`。
- README 后续补充 watch 命令。

风险：

- 如果引入 npm 依赖，仓库是否提交 `node_modules` 必须明确。建议不提交。

验证：

- `npm run watch` 或 `pnpm watch` 后修改 TS，dist JS 更新。
- UE reload 后看到行为变化。

### 7. README 和 demo 文档更新

目标：

- 将可运行结果写入 README，把实现细节保留在本文档。

涉及文件：

```text
README.md
README.zh-CN.md
README.ja.md
docs/project-map.md
docs/project-map.zh-CN.md
docs/project-map.ja.md
```

内容：

- README 增加 demo 运行方法、watch 方法、reload command、DevTools 调试方式。
- project-map 增加 Runtime/Bridge/ScriptHost 新结构。

风险：

- 多语言 README 内容容易不一致。建议先更新英文和中文，日文后续对齐。

验证：

- 按 README 步骤可以复现 demo。

## 最小验收标准

- UE 启动后成功加载 manifest 指定 main module。
- `start` 被调用。
- JS 可以 spawn actor。
- UE 每帧调用 JS `tick(deltaTime)`。
- actor 可以在 JS 控制下移动、浮动或旋转。
- `dispose` 可以清理 actor。
- `RinRinJs.Reload` 可以重新加载脚本。
- 修改 TypeScript 参数后，不重启 UE Editor 即可看到变化。
- Chrome DevTools 可以在 `tick` 中断点调试。
- JS 错误不会导致 UE 崩溃，而是输出清晰错误和 stack trace。

## 后续演进方向

v0 完成后，可以逐步演进：

- manifest v0 -> package registry / mod manager。
- opaque actor handle -> 更完整的 UObject wrapper。
- `globalThis.ue` -> permissioned native API namespace。
- GameInstance ticker -> Subsystem 或 plugin runtime host。
- reload v0 -> 更细粒度 script reload / HMR。
- 调试 dist JS -> source map 支持。

重点是：v0 不是一次性废弃原型，而是用一个可替换的 host/bridge 外壳包住现有 runtime core。未来扩展时优先替换外壳和组织层，不要重写 V8、Inspector、错误模型这些底座。
