# RinRinJs-Lab / RinRinJs

`RinRinJs-Lab` 是一个基于 Unreal Engine 5.7 的示例工程，核心内容是项目内插件 `RinRinJs`。这个插件将 Google V8 嵌入 Unreal Engine，使 C++ 游戏代码能够加载、执行并调试 JavaScript。

这个仓库主要作为作品集和源码展示材料使用，用来体现我在原生引擎集成、第三方运行时嵌入、Unreal 模块边界设计、错误处理以及开发调试工具方面的实现方式。它本质上是一个探索性项目，而不是一个承诺长期维护的产品化插件，因此我不提供兼容性保证，也不承诺面向外部使用的长期支持。

语言版本：

- English: [README.md](README.md)
- 简体中文：`README.zh-CN.md`
- 日本語: [README.ja.md](README.ja.md)

## 项目目标

这个项目的目标，是探索一个适用于 Unreal Engine 的 JavaScript 运行时层，并逐步支持以下能力：

- 从 C++ 驱动 JavaScript 进行游戏逻辑脚本化；
- 支持 Mod 或用户自定义脚本工作流；
- 从项目内容目录加载 ES Module；
- 通过 V8 Inspector / Chrome DevTools 进行浏览器调试；
- 提供结构化的 C++ 错误返回，而不是只依赖日志；
- 为未来的 JavaScript 与 Unreal 对象、函数、游戏系统互操作打基础。

当前阶段优先解决最困难的部分：在 Windows + MSVC 环境下把 V8 稳定嵌入 UE Runtime 模块。V8 在这个环境中的集成难点比较集中，包括编译参数、CRT/链接一致性、Unreal Build Tool 交互，以及平台宏和编译器宏对齐。因此当前仅支持 Windows 运行。

## 当前状态

已经实现：

- Unreal 运行时插件 `RinRinJs`
- V8 的进程级初始化与关闭
- V8 isolate/context 的创建与释放
- 从 C++ 直接执行 JavaScript
- ES Module 加载与依赖解析
- `Content/Mods/Core` 下的示例脚本
- 基于 WebSocket 的 V8 Inspector，可供 Chrome/Edge DevTools 调试
- `/json/list` 等本地 Inspector 发现接口
- C++ 与 JS 之间的值封装
- 基于 `TExpected` 和 `FError` 的结构化错误/结果模型
- 一个最小可运行的 `GameInstance` 集成示例

仍在探索或尚未完成：

- package/mod registry 与 manifest 加载
- 用类型化参数调用 JS 导出函数的稳定公共 API
- UObject / UFunction 绑定
- promise / async 与 UE tick 或 latent action 的集成
- 跨平台 V8 构建
- 面向真实项目的打包与分发流程

当前不作为目标的内容：

- Marketplace 级别的插件发布形态
- 向后兼容的公共 API 承诺
- 多平台二进制分发
- 完整的 Node.js 兼容层或大而全的 JS 标准库环境

## 平台支持

当前目标平台：

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- 以 monolithic static library 方式链接 V8

仓库中包含 V8 头文件和 Windows Release 静态库：

```text
Plugins/RinRinJs/ThirdParty/v8
```

当前 V8 采用的是面向 MSVC/Win64 的单体静态库构建。关键构建参数如下：

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

## 打开与编译

正常情况下，这个项目应当被视为一个标准的 Unreal Engine 工程。编译由 Unreal Engine 和 Unreal Build Tool 负责，而不是依赖某个特定 IDE。

推荐环境：

- Unreal Engine 5.7
- 安装了 C++ 桌面开发工具链的 Visual Studio 2022
- 与当前 UE 工具链兼容的 Windows SDK

clone 之后通常的首次流程：

1. 确认 `Plugins/RinRinJs/ThirdParty/v8` 中包含仓库附带的 V8 头文件和 Win64 静态库。
2. 使用 Unreal Engine 5.7 打开 `RinRinJsLab.uproject`。
3. 如果 Unreal 提示生成项目文件，则允许它生成。
4. 从 Unreal Editor 中编译项目，或者在首次打开时让 Unreal 自动触发编译。

说明：

- 日常编译可以直接在 Unreal Editor 内完成。
- Visual Studio 和 VSCode 都只是围绕同一套 UBT 构建链路的可选开发环境。
- 只有在你希望使用 VSCode 的 workspace、IntelliSense 或 task 时，才需要额外生成 VSCode 相关工程文件。

## 运行方式

示例工程当前通过 `URinRinJsLabGameInstance` 启动 JavaScript 运行时。

启动流程：

1. Unreal 调用 `URinRinJsLabGameInstance::Init()`。
2. 通过 `FModuleManager` 获取 `RinRinJs` 模块。
3. 调用 `FRinRinJsModule::StartRuntime()`。
4. 插件初始化 V8，并创建执行上下文。
5. 游戏侧加载 `"main"` JavaScript 模块。
6. 游戏侧执行一个测试表达式，目前是 `foo(2, 3)`。

关闭流程：

1. `URinRinJsLabGameInstance::Shutdown()` 再次获取插件模块。
2. 调用 `FRinRinJsModule::StopRuntime()`。
3. 插件释放已加载模块、Inspector、V8 context、isolate、allocator，以及进程级 V8 资源。

当前示例中的入口模块解析为：

```text
main -> Content/Mods/Core/main.js
```

其余 ESM 相对导入则以导入方模块所在目录为基准进行解析。

## 基本调用方式

当前对外的 C++ 入口是 `FRinRinJsModule`。

示例 `GameInstance` 中的调用方式如下：

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

宿主游戏当前需要提供两个 callback：

- `FResolveModuleIdFn`：把 import specifier 解析成模块 id，目前是标准化后的文件路径
- `FLoadSourceByModuleIdFn`：按照模块 id 读取 UTF-8 JavaScript 源码

这种设计让文件系统策略保留在宿主层，而不是写死在 V8 层。后续如果要接入 pak、下载内容、虚拟文件系统或编辑器资源，也更容易扩展。

## JavaScript 示例

当前示例脚本位于：

```text
Content/Mods/Core
```

`main.js`：

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

`utils.js`：

```js
function bar(x) {
    return x * 2;
}

globalThis.bar = bar;

export { bar };
```

这里保留 `globalThis.foo = foo`，是因为当前直接求值路径仍然可以在 ESM 执行完成后调用全局函数。更理想的内部演进方向，是从 C++ 直接调用模块导出，并以类型化 API 传递参数。

## 使用 Chrome DevTools 调试

当 V8 context 创建完成后，插件会同时启动 V8 Inspector transport。

默认本地端点：

```text
ws://127.0.0.1:9229/
```

发现接口：

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

基本调试流程：

1. 在 Windows 上运行 Unreal 工程
2. 打开 Chrome 或 Edge
3. 访问 `chrome://inspect`
4. 配置或检查 `127.0.0.1:9229`
5. 连接暴露出来的 V8 target

当前 Inspector transport 默认仅允许本地访问。底层 HTTP/WebSocket 使用 CivetWeb，协议消息通过 Unreal ticker 驱动。

## 项目结构

更详细的源码结构、分层说明和依赖流向请见：

- English: [docs/project-map.md](docs/project-map.md)
- 简体中文：[docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](docs/project-map.ja.md)

## 设计说明

当前实现有意保留了几条清晰边界：

- Unreal 面向上层的 API 放在 `Public`
- V8 相关类型尽量留在 `Private`
- 模块解析与源码读取策略由宿主游戏负责
- 运行错误通过 `TExpected` 返回，而不是只写日志
- 浏览器调试能力被视为运行时体验的一部分，而不是事后补上的工具

对于作品集项目来说，这种组织方式不仅展示了“功能是否可用”，也展示了我如何处理复杂原生依赖在大型引擎中的嵌入问题。

## 后续计划

接下来大概率会继续推进：

- 完成 package/mod registry 与 manifest 设计
- 把稳定的 `ExecuteJsFunction` API 暴露到 `FRinRinJsModule`
- 直接调用 ES Module 的导出，而不是依赖全局函数
- 扩展 `FValueIntoJs` / `FValueFromJs`，支持数组、对象和 Unreal 引用
- 增加 UObject / UFunction 绑定
- 处理 promise / microtask 与 Unreal tick 语义之间的关系
- 改善 TypeScript 与 source map 调试体验
- 清理旧的 ChakraCore 历史代码，或将其转入文档说明
- 增补 Windows 下的编译和 smoke test 文档
