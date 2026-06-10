# Project Map

本文档描述 `RinRinJs-Lab` 和 `RinRinJs` 插件当前的源码结构、运行时分层和依赖流向。

语言版本：

- English: [docs/project-map.md](project-map.md)
- 简体中文：`docs/project-map.zh-CN.md`
- 日本語: [docs/project-map.ja.md](project-map.ja.md)

## 顶层结构

```text
RinRinJsLab.uproject
Config/
Source/RinRinJsLab/
Content/Mods/Core/
Plugins/RinRinJs/
```

职责：

- `RinRinJsLab.uproject` 定义 Unreal 工程。
- `Config/DefaultEngine.ini` 设置示例地图和 `URinRinJsLabGameInstance`。
- `Source/RinRinJsLab` 是宿主游戏模块。
- `Content/Mods/Core` 是运行时加载的脚本包。
- `Plugins/RinRinJs` 是嵌入 V8 的运行时插件。

## 宿主游戏模块

```text
Source/RinRinJsLab/
  RinRinJsLab.Build.cs
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
```

职责：

- 依赖 `RinRinJs` 插件模块。
- 启动和停止插件运行时。
- 注册 ticker，更新插件持有的 world，并驱动 JS tick。
- 等待游戏 world 进入 play 后再加载脚本包。
- 将 `Content/Mods/Core` 作为当前活动脚本包加载。

当前流程：

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

## 脚本包

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

职责：

- `rinrin.manifest.json` 定义 package 元数据和 main module。
- `package.json` 将目录标记为 ESM，方便编辑器/工具识别。
- `main.js` 导出生命周期函数。

Manifest：

```json
{
    "name": "core-demo",
    "version": "0.1.0",
    "main": "main.js"
}
```

生命周期导出：

```text
start(context)
tick(deltaSeconds)
dispose()
```

## 插件描述与构建

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

职责：

- 声明 `RinRinJs` 为 Runtime 插件模块。
- 添加 `Core`、`CoreUObject`、`Engine`、`Json`、`Projects` 等 Unreal 依赖。
- 添加 V8 include/library 路径。
- 定义与 Win64 静态 V8 构建匹配的编译宏。
- 添加 CivetWeb 所需的本地 Inspector HTTP/WebSocket transport 宏。

## 插件 Public API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
    Error.h
    Expected.h
    Log.h
```

职责：

- `RinRinJs.h` 声明 `FRinRinJsModule`，也就是 Unreal 侧插件入口。
- `ModuleResolver.h` 声明宿主提供的 module resolve/load callback 类型。
- `Util/Expected.h` 定义成功或错误的返回值。
- `Util/Error.h` 定义带源码位置和可选 JS stack 的结构化错误。
- `Util/Log.h` 定义插件日志辅助工具。

重要边界：

- Public 头文件避免暴露 V8 类型。
- 运行时和 bridge 的实现细节保留在 `Private`。

## 插件模块

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

职责：

- 实现 `FRinRinJsModule`。
- 在模块启动时初始化进程级 V8 状态。
- 通过显式 runtime call 启动和停止执行上下文状态。
- 向宿主游戏暴露脚本包生命周期 API。
- 注册和注销 `RinRinJs.Reload` 控制台命令。

宿主当前使用的模块调用：

```text
StartRuntime()
StopRuntime()
SetGameWorld(UWorld*)
LoadScriptPackage(packageRoot)
UnloadScriptPackage()
ReloadScriptPackage()
TickRuntime(deltaSeconds)
```

## Runtime 层

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Runtime/
  ScriptHost.h
  ScriptHost.cpp
  ScriptManifest.h
  ScriptManifest.cpp
```

职责：

- 加载 `rinrin.manifest.json`。
- 规范化 package root 和 main module 路径。
- 将 module resolve 限制在 package root 内。
- 创建每个 package 对应的 native bridge 和 actor registry。
- 向 V8 context 注入 `globalThis.ue`。
- 加载并执行 main ES module。
- 在导出存在时调用 `start(context)`、`tick(deltaSeconds)`、`dispose()`。
- reload 时重建 V8 execution context。
- unload/reload 时销毁 JS 创建的 actor。

流程：

```text
FRinRinJsModule
  -> FScriptHost
    -> LoadScriptManifest
    -> FNativeBridge
    -> FActorHandleRegistry
    -> FV8Loader
    -> FV8ModuleManager
```

## Bridge 层

```text
Plugins/RinRinJs/Source/RinRinJs/Private/Bridge/
  NativeBridge.h
  NativeBridge.cpp
  ActorHandleRegistry.h
  ActorHandleRegistry.cpp
```

职责：

- 向 JavaScript 注入 `ue` 对象。
- 在调用 Unreal API 前校验 JS 参数。
- 将简单 JS object 转换为 `FVector`、`FRotator`、`FTransform`。
- 通过 `UStaticMesh` asset path 创建 `AStaticMeshActor`。
- 使用 `RinRinJsDemoActor` 前缀命名创建出来的 actor，方便在 World Outliner 中查找。
- 用 opaque integer handle 保存 actor 引用。
- 根据 handle 找回 actor，用于后续 bridge 调用。
- unload 时销毁所有 registry 记录的 JS actor。

当前 bridge API：

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

## V8 层

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Loader.h
  V8Loader.cpp
  V8ModuleManager.h
  V8ModuleManager.cpp
  V8Console.h
  V8Console.cpp
```

职责：

- 初始化和释放进程级 V8 资源。
- 持有 V8 platform、allocator、isolate、execution context。
- 创建和销毁 V8 context。
- 创建并暴露 `FV8ModuleManager`。
- 执行直接传入的 JavaScript source string。
- 通过宿主 resolver/source-loader callback 加载 ES Module。
- 执行 module 导出的函数。
- 避免在 public plugin header 中暴露 V8 细节。

概念流程：

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

## Inspector 层

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

职责：

- 通过本地 WebSocket 暴露 Chrome DevTools Protocol。
- 提供 `/json`、`/json/list`、`/json/version` discovery endpoint。
- 创建和管理 V8 Inspector session。
- 将 DevTools 消息转发给 V8。
- 将 JS console 输出转发到 Unreal log。
- 通过 Unreal ticker pump transport 消息。

依赖流向：

```text
FV8Loader
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## Utility 层

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

职责：

- 通过 `TExpected` 返回失败。
- 通过 `FError` 保留结构化错误上下文。
- 在可用时捕获和输出 JS stack。
- 用源码位置格式化日志。

## 第三方依赖

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/Source/ChakraCoreLoader
```

职责：

- V8 提供 JavaScript 引擎和 Inspector API。
- CivetWeb 提供 DevTools 所需的本地 HTTP/WebSocket transport。
- ChakraCoreLoader 作为较早 runtime loader 工作保留；当 `RinRinJs_USE_V8=1` 时不是当前运行路径。

## 总体依赖流向

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

## 开发备注

- `Content/Mods/Core/main.js` 是最快的脚本行为迭代入口。
- `RinRinJs.Reload` 是当前运行时重载入口。
- Actor handle 有意保持 opaque，JavaScript 不应拿到裸 UObject 指针。
- 脚本加载应继续拒绝 package root 之外的 import。
- 依赖 world 的脚本加载应发生在 game world begun play 之后。
