# RinRinJs-Lab / RinRinJs

语言版本：

- English: [README.md](README.md)
- 简体中文：`README.zh-CN.md`
- 日本語: [README.ja.md](README.ja.md)

## 维护说明

这个仓库目前主要用于作品集展示和招聘评估。我现阶段无法为它提供持续维护、功能承诺或对外支持。

`RinRinJs-Lab` 是一个 Unreal Engine 5.7 示例工程，核心是运行时插件 `RinRinJs`。插件将 Google V8 嵌入 Unreal Engine，可以从项目内容目录加载 ES Module 脚本包，向 JavaScript 暴露受控的 Unreal API，按游戏帧驱动脚本生命周期，并通过 Chrome/Edge DevTools 调试脚本。

这个仓库主要作为作品集和源码展示材料，用来体现原生引擎集成、第三方运行时嵌入、Unreal 模块边界、结构化错误处理、浏览器调试，以及脚本驱动玩法循环的实现方式。

演示视频：展示 JavaScript 代码修改、Unreal 运行画面、hot reload，以及使用 Chrome DevTools 进行调试。

https://github.com/user-attachments/assets/be83b3a8-c648-420e-9dd5-5a582635e4c4

## 当前状态

已经实现：

- Unreal 运行时插件模块 `RinRinJs`。
- V8 进程级初始化与关闭。
- V8 isolate/context 创建与清理。
- 从 C++ 直接执行 JavaScript。
- ES Module 加载、依赖解析和模块缓存处理。
- 通过 `rinrin.manifest.json` 加载脚本包。
- 脚本生命周期：`start(context)`、`tick(deltaSeconds)`、`dispose()`。
- `URinRinJsLabGameInstance` 集成：启动运行时，等待游戏世界进入 play，再加载脚本包并逐帧 tick JS。
- 向 JavaScript 注入小型 `ue` bridge。
- 通过 opaque actor handle 创建和控制 actor。
- 通过控制台命令 `RinRinJs.Reload` 运行时重载脚本。
- 基于 WebSocket 的 V8 Inspector，可使用 Chrome/Edge DevTools 调试。
- `/json/list` 等本地 Inspector discovery endpoint。
- 基于 `TExpected` 和 `FError` 的结构化错误/结果模型。

当前 JavaScript bridge：

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

当前不提供：

- 稳定的公共 API 兼容性承诺。
- Marketplace 级别插件打包。
- Node.js 兼容层。
- 跨平台 V8 二进制。
- 通用 UObject/UFunction 反射。
- 完整 package/mod manager。

## 平台支持

当前目标平台：

- Windows
- Unreal Engine 5.7
- Visual Studio 2022 / MSVC
- Win64
- C++20
- V8 以 monolithic static library 方式链接

仓库期望 V8 头文件和 Win64 release 静态库位于：

```text
Plugins/RinRinJs/ThirdParty/v8
```

## 打开与编译

推荐环境：

- Unreal Engine 5.7
- 安装 C++ 桌面开发工具链的 Visual Studio 2022
- 与当前 UE 工具链兼容的 Windows SDK

首次运行流程：

1. 确认 `Plugins/RinRinJs/ThirdParty/v8` 中存在 V8 头文件和 Win64 库。
2. 使用 Unreal Engine 5.7 打开 `RinRinJsLab.uproject`。
3. 如果 Unreal 提示生成项目文件，允许生成。
4. 从 Unreal Editor、Visual Studio 或 Unreal Build Tool 编译项目。

## 运行流程

游戏模块通过 `URinRinJsLabGameInstance` 启动并驱动运行时。

启动流程：

1. `URinRinJsLabGameInstance::Init()` 获取 `RinRinJs` 模块。
2. 游戏调用 `FRinRinJsModule::StartRuntime()`。
3. 插件初始化 V8 并创建执行上下文。
4. 游戏 ticker 持续更新插件持有的 `UWorld`。
5. 当 world 是 game world 且 `HasBegunPlay()` 为 true 后，加载 `Content/Mods/Core`。
6. `FScriptHost` 读取 `rinrin.manifest.json`，加载 manifest 指定的 `main` module，注入 `globalThis.ue`，并调用 `start({ packageName })`。
7. 每帧调用导出的 `tick(deltaSeconds)`，如果该函数存在。

关闭流程：

1. `URinRinJsLabGameInstance::Shutdown()` 移除 ticker。
2. 游戏卸载当前脚本包。
3. `FScriptHost` 调用 `dispose()`，销毁 JS 创建的 actor，并清理 actor handle。
4. 插件停止运行时并释放 V8 context/process 状态。

## 脚本包

当前脚本包位于：

```text
Content/Mods/Core/
  rinrin.manifest.json
  package.json
  main.js
```

`rinrin.manifest.json`：

```json
{
    "name": "core-demo",
    "version": "0.1.0",
    "main": "main.js"
}
```

`main.js` 导出生命周期函数：

```js
export function start(context) {
    actor = ue.spawnActorByPath("/Engine/BasicShapes/Cube.Cube", {
        location: { x: 200, y: 0, z: 120 },
        rotation: { pitch: 0, yaw: 0, roll: 0 },
        scale: { x: 1, y: 1, z: 1 },
    });
}

export function tick(deltaSeconds) {
    // 移动或旋转 actor。
}

export function dispose() {
    if (actor) {
        ue.destroy(actor);
        actor = 0;
    }
}
```

当前 workflow 直接加载 JavaScript 源文件，不需要 TypeScript 或打包步骤。

## 重载脚本

插件注册了控制台命令：

```text
RinRinJs.Reload
```

可以从以下位置执行：

- 游戏内控制台，通常按 `~` 打开；
- Unreal Editor 的 Output Log 命令输入框。

重载流程：

1. 如果当前 main module 导出了 `dispose()`，先调用它。
2. 销毁 actor handle registry 中记录的 JS actor。
3. 重建 V8 execution context。
4. 重新读取 manifest 和源码文件。
5. 加载并执行 main module。
6. 再次调用 `start(context)`。

因此可以直接编辑 `Content/Mods/Core/main.js`，执行 `RinRinJs.Reload`，不重启编辑器即可看到行为变化。

## 使用 Chrome DevTools 调试

当 V8 context 创建后，插件会启动 V8 Inspector transport。

默认本地 WebSocket endpoint：

```text
ws://127.0.0.1:9229/
```

推荐直接打开的 DevTools URL：

```text
devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/
```

Discovery endpoints：

```text
http://127.0.0.1:9229/json
http://127.0.0.1:9229/json/list
http://127.0.0.1:9229/json/version
```

推荐流程：

1. 运行 Unreal 项目。
2. 打开 Chrome 或 Edge。
3. 直接访问 `devtools://devtools/bundled/js_app.html?ws=127.0.0.1:9229/`。
4. 直接连接本地 V8 target。

Inspector transport 默认只面向本地访问，底层 HTTP/WebSocket 使用 CivetWeb。

`chrome://inspect` 仍然可以作为 discovery 入口，但从那里点击 `Inspect` 打开的独立窗口，在断开连接或 reload 后未必会像直接 `devtools://...` 一样清晰反映当前状态。

## 项目结构

更详细的源码结构、运行时分层和依赖流向请见：

- English: [docs/project-map.md](docs/project-map.md)
- 简体中文：[docs/project-map.zh-CN.md](docs/project-map.zh-CN.md)
- 日本語: [docs/project-map.ja.md](docs/project-map.ja.md)

## 设计说明

当前实现保留了几条清晰边界：

- `FRinRinJsModule` 是 Unreal 侧插件入口。
- V8 相关实现留在 `Private/V8`。
- 脚本包生命周期位于 `Private/Runtime`。
- JS 到 UE 的调用白名单位于 `Private/Bridge`。
- 脚本文件解析被限制在 package root 内。
- 运行时错误尽量通过 `TExpected` 和 `FError` 返回。
- 暴露给 JS 的 actor 引用是整数 handle，不是裸 UObject 指针。

## 后续计划

可能的下一步：

- 添加一个小型 debug UI，用于触发 reload 等常用运行时命令。
- 将脚本包加载扩展为 package/mod registry，并明确 load order。
- 定义 JS 创建 actor 与共享/全局 actor 的 ownership 策略。
- 在保持显式授权的前提下扩展 bridge，不局限于 `AStaticMeshActor`。
- 增加数组、对象、Unreal 引用等 typed C++/JS value conversion。
- 在明确 allowlist 后加入 UObject/UFunction 绑定。
- 改善 reload 与 DevTools session、状态检查之间的交互。
- 补充 Windows 自动编译和 smoke test 文档。
- 决定旧 ChakraCore 代码作为历史保留，还是从 active plugin 中移除。
