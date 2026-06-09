# Project Map

本文档说明 `RinRinJs-Lab` 以及 `RinRinJs` 插件的主要源码结构、分层方式和依赖流向。

语言版本：

- English: [docs/project-map.md](project-map.md)
- 简体中文：`docs/project-map.zh-CN.md`
- 日本語: [docs/project-map.ja.md](project-map.ja.md)

## 顶层结构

```text
RinRinJsLab.uproject
Source/RinRinJsLab
Content/Mods/Core
Plugins/RinRinJs
```

职责：

- `RinRinJsLab.uproject` 定义 Unreal 工程本体
- `Source/RinRinJsLab` 是示例游戏模块
- `Content/Mods/Core` 保存由运行时加载的 JavaScript 示例
- `Plugins/RinRinJs` 是嵌入 V8 的 Unreal 插件

## 游戏模块

```text
Source/RinRinJsLab/
  RinRinJsLabGameInstance.h
  RinRinJsLabGameInstance.cpp
  RinRinJsLab.Build.cs
```

职责：

- 持有示例的启动/关闭流程
- 调用 `FRinRinJsModule`
- 提供模块路径解析
- 提供 JavaScript 源码读取
- 展示游戏逻辑代码如何接入插件

当前依赖关系：

```text
RinRinJsLab.Build.cs
  -> PrivateDependencyModuleNames
    -> RinRinJs
```

## 插件描述与构建

```text
Plugins/RinRinJs/
  RinRinJs.uplugin
  Source/RinRinJs/RinRinJs.Build.cs
```

职责：

- 声明 `RinRinJs` 为 Runtime 插件模块
- 添加 `Core`、`CoreUObject`、`Projects` 等 Unreal 依赖
- 添加 V8 的 include 和 library 路径
- 添加与当前 Win64 静态 V8 构建一致的编译宏
- 添加 CivetWeb 所需的 WebSocket 编译宏

## 插件 Public API

```text
Plugins/RinRinJs/Source/RinRinJs/Public/
  RinRinJs.h
  ModuleResolver.h
  Util/
  Value/
```

职责：

- `RinRinJs.h`：声明 `FRinRinJsModule`，作为 Unreal 侧主入口
- `ModuleResolver.h`：声明宿主提供的模块解析/加载 callback 类型
- `Value/ValueIntoJs.h`：定义从 C++ 传入 JS 的值容器
- `Value/ValueFromJs.h`：包装从 JS 返回给 C++ 的值
- `Util/Expected.h`：定义成功值或错误的返回结构
- `Util/Error.h`：定义带源码位置和可选堆栈信息的结构化错误
- `Util/Log.h`：定义插件日志辅助工具

重要边界：

- Public 头文件尽量不要泄露 V8 类型
- V8 相关实现尽量留在 `Private` 中，由插件侧包装后再暴露

## 插件模块实现

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  RinRinJs.cpp
```

职责：

- 实现 `FRinRinJsModule`
- 在模块启动时初始化进程级 V8 状态
- 在显式 start/stop 时创建和销毁运行时状态
- 向游戏层暴露模块加载和脚本求值入口

## V8 Runtime 层

```text
Plugins/RinRinJs/Source/RinRinJs/Private/V8/
  V8Runtime.h
  V8Runtime.cpp
  V8EsModuleLoader.h
  V8EsModuleLoader.cpp
  V8Includes.h
```

职责：

- 管理 V8 platform 初始化
- 管理 V8 isolate/context 生命周期
- 执行直接传入的 JavaScript 字符串
- 加载并执行 ES Modules
- 缓存编译后的模块
- 将 V8 值转换为对外的包装类型

概念流向：

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

职责：

- 通过宿主 callback 解析模块 specifier
- 通过宿主 callback 加载 JavaScript 源码
- 用 `v8::ScriptCompiler::CompileModule` 编译模块
- 用 `v8::Module::InstantiateModule` 实例化模块
- 用 `v8::Module::Evaluate` 执行模块
- 按 resolved id 缓存模块
- 记录 referrer 模块 id，支持相对 import 解析

宿主 callback：

```text
FResolveModuleIdFn
  (referrer id, import specifier) -> resolved module id

FLoadSourceByModuleIdFn
  (resolved module id) -> UTF-8 source
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

- 通过本地 WebSocket 暴露 Chrome DevTools Protocol
- 提供 `/json`、`/json/list`、`/json/version` 发现接口
- 创建和管理 V8 Inspector session
- 将 DevTools 消息桥接到 V8
- 将 JS console 输出转发到 Unreal log
- 由 Unreal ticker 驱动 transport 消息循环

依赖流向：

```text
FV8Runtime
  -> FV8Inspector
    -> FV8InspectorHost
      -> v8_inspector::V8Inspector
      -> v8_inspector::V8InspectorSession
    -> FV8InspectorTransport
      -> CivetWeb
```

## Value 与 Utility 层

```text
Plugins/RinRinJs/Source/RinRinJs/Public/Value/
Plugins/RinRinJs/Source/RinRinJs/Private/Value/
Plugins/RinRinJs/Source/RinRinJs/Public/Util/
Plugins/RinRinJs/Source/RinRinJs/Private/Util/
```

职责：

- 在 V8 值与对外值包装之间转换
- 把 V8 handle 和 context 细节留在私有实现中
- 通过 `TExpected` 返回失败信息
- 用 `FError` 保留结构化错误细节
- 用源码位置信息格式化日志

当前值类型：

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

## Package Registry 工作区

```text
Plugins/RinRinJs/Source/RinRinJs/Private/
  PackageLoader.h
  PackageRegistry.h
  PackageRegistry.cpp
```

当前意图：

- 发现 package manifest
- 缓存 package 元数据
- 按 load order 排序 package
- 通过 loader interface 委托具体加载动作

这一块目前仍处于探索阶段，还不是稳定的运行时功能。

## 第三方依赖

```text
Plugins/RinRinJs/ThirdParty/v8
Plugins/RinRinJs/Source/RinRinJs/Private/ThirdParty/civetweb
Plugins/RinRinJs/ThirdParty/ChakraCore
```

职责：

- V8 提供 JavaScript 引擎和 Inspector API
- CivetWeb 提供 DevTools 所需的本地 HTTP/WebSocket 传输层
- ChakraCore 保留为更早期的探索或备选资料

## 总体依赖流向

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
