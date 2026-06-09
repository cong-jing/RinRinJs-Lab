# RinRinJs-Lab 下一阶段实现计划整理请求

## 项目背景

当前项目名称是 RinRinJs-Lab，其中包含一个演示用的 Unreal Engine 工程；核心插件名称是 RinRinJs。

项目目标是在 Unreal Engine 中嵌入 JavaScript runtime，使用 V8 执行 JS/TS 编译后的脚本，并支持 Chrome DevTools 调试。这个项目既是个人技术验证，也是求职展示用作品，重点展示以下能力：

* Unreal Engine / C++ 插件开发
* V8 runtime 集成
* JS module 执行
* Chrome DevTools / V8 Inspector 调试集成
* UE 内部 scripting layer / prototype layer 的可能性
* 开发时快速迭代体验

目前项目已经公开到 GitHub，并已放入简历中作为个人开发项目展示。

## 当前已完成的大致阶段

请先根据仓库实际代码确认以下内容是否准确，并补充遗漏点：

1. V8 已经可以在 UE 插件中运行，Release 构建下相对稳定。
2. 已经实现基本 JS 执行能力。
3. 已经集成 Chrome DevTools / V8 Inspector，可以连接 Chrome 调试 JS。
4. 已有基础 module load / execute 相关代码。
5. 当前还没有完成真正可用的 JS ⇄ UE bridge。
6. 当前还没有量产级 UObject wrapper。
7. 当前还没有实现从 JS spawn actor、控制 actor transform 等可见 UE 场景行为。
8. 当前项目更接近 runtime / inspector 技术验证，还缺少一个招聘方一眼能看懂的视频 demo。

## 下一阶段核心目标

下一阶段目标不是完整 package manager，也不是完整 UObject reflection，而是完成一个最小可展示垂直切片：

JS-driven Actor Demo v0

这个 demo 要证明：

UE 正在运行中，JS module 可以控制 UE 场景里的 actor；UE 每帧调用 JS tick；修改 TypeScript 参数后可以重新编译并 reload script，不重启 UE 工程即可看到行为变化；同时 Chrome DevTools 可以在 JS tick 中打断点调试。

这条展示链路比完整架构更重要：

UE 中运行 JS
→ JS 调用受控 UE API
→ spawn actor
→ tick 中更新 transform
→ 修改 TS 参数
→ tsc watch 编译
→ UE 中 reload script
→ actor 行为变化
→ Chrome DevTools 断点调试
→ JS 错误不会导致 UE 崩溃，而是输出清晰错误和 stack trace

## 为什么不优先做完整 package manager

之前考虑过 package manager / mod 依赖管理，因为它会影响 module 边界、脚本包结构和未来 mod 设计。

但短期求职展示中，完整 package manager 的可视化价值不如“JS 控制 UE 场景对象”的 demo。package manager、mod dependency resolution、semver、lock file、load order、权限系统、mod conflict 等都很重要，但容易让项目陷入长期架构设计，短期不利于做出可展示成果。

因此 package manager 方向暂时降级为：

manifest-based module loader v0

只需要支持非常简单的 manifest，例如：

* name
* version
* main
* package root

manifest v0 的目的不是完整依赖管理，而是明确脚本包边界和入口，让项目看起来不是随便 eval 一个 JS 文件，而是具有未来 script package / mod package 扩展方向。

## 推荐实现方向

### 1. module lifecycle v0

加载 JS module 后，检测并调用以下 export：

* start(context)
* tick(deltaTime)
* dispose()

其中：

* start 在 module 首次加载后调用
* tick 由 UE plugin 每帧调用
* dispose 在 reload 或 shutdown 时调用

第一阶段不要设计 JS class extends Actor，也不要做 ScriptComponent framework。先只做 module-level lifecycle。

### 2. ActorHandle v0

第一阶段不要做完整 UObject wrapper。

JS 侧拿到的不是 UObject，而是 opaque actor handle。

C++ 侧使用 TWeakObjectPtr<AActor> 或类似方式管理实际 actor。每次 JS 调用 actor API 时检查对象是否仍然有效。如果 actor 已被销毁，应该返回清晰错误，而不是崩溃。

ActorHandle 可以是数字 ID、字符串 ID 或 JS object wrapper，具体请根据当前项目代码结构提案。

### 3. 白名单 UE API v0

第一阶段只暴露少量受控 API，避免自动 reflection。

建议 API 包括：

* log(message)
* spawnActorByPath(assetPath, transform?)
* destroy(actorHandle)
* setLocation(actorHandle, location)
* getLocation(actorHandle)
* setRotation(actorHandle, rotation)
* setTransform(actorHandle, transform)
* addWorldOffset(actorHandle, offset)
* setVisible(actorHandle, visible)

可以根据当前项目实际结构调整，但不要一开始暴露所有 UObject property / method。

### 4. Transform / Vector 数据结构

请根据当前 JS binding 现状提案最小数据结构。

可以先使用 plain object：

* Vector: { x, y, z }
* Rotator: { pitch, yaw, roll }
* Transform: { location, rotation, scale }

也可以根据现有 C++ / JS value conversion 代码设计更适合的格式。

第一阶段重点是可用、可调试、可展示，不要过早设计复杂类型系统。

### 5. Reload command v0

需要支持 UE 运行中触发 reload。

建议提供 console command，例如：

RinRinJs.Reload

reload 流程建议：

1. 调用当前 module 的 dispose。
2. 清理旧 actor handles。
3. 销毁 demo 中由 JS 创建的 actor。
4. 释放或重建 module 相关 JS references。
5. 重新读取 manifest。
6. 重新加载 main module。
7. 调用 start。
8. 后续 Tick 调用新的 tick。

reload 出错时：

* UE 不崩溃。
* 输出 JS stack trace 或清晰错误信息。
* 尽量保持 plugin 状态可继续 reload。
* 不要残留旧 actor 或无效 handle。

### 6. TypeScript watch workflow

目标是让用户可以修改 TS 后快速看到 UE 场景变化。

推荐流程：

1. scripts/src/demo.ts 修改参数，例如 radius / speed / height。
2. tsc watch 自动编译到 scripts/dist 或插件指定运行目录。
3. UE 中执行 RinRinJs.Reload。
4. 不重启 UE Editor。
5. actor 行为立即变化。

请根据当前项目目录结构提案实际路径。

需要注意：

* JS runtime 应加载编译后的 JS，而不是直接加载 TS。
* README 中需要说明 watch 命令。
* demo 代码应尽量短，便于视频展示。

### 7. Chrome DevTools demo

已有 V8 Inspector / Chrome DevTools 集成。

下一阶段需要确保：

* 可以在 demo module 的 tick 函数中打断点。
* UE 运行时可以停在 JS 代码里。
* 可以看到 radius / speed / elapsedTime / actorHandle 等变量。
* reload 后 DevTools 行为是否稳定，需要根据当前实现确认。
* 若 reload 后 source mapping 或脚本路径存在问题，请提出最小修复方案。

source map 不是第一优先级。如果当前成本较高，可以先接受调试编译后的 JS；但请在提案中说明是否建议加入 source map 支持。

## Demo 行为建议

最小 demo 可以是：

1. start 中 spawn 一个 cube / sphere / blueprint actor。
2. 保存 actor handle。
3. tick(deltaTime) 中累积 time。
4. 根据 radius / speed 计算位置。
5. 让 actor 绕圈移动。
6. 叠加上下浮动。
7. 可选：持续旋转 actor。
8. dispose 中 destroy actor。

TS 参数示例：

* radius
* speed
* heightAmplitude
* rotationSpeed

视频中可以修改 radius 或 speed，保存后 tsc watch 编译，再在 UE 中 reload，展示轨迹变化。

## 非目标

以下内容暂时不要作为下一阶段主线：

1. 完整 package manager / mod dependency management。
2. semver / lock file / remote package。
3. 完整 UObject reflection。
4. BlueprintCallable 自动暴露。
5. JS class extends Actor。
6. 完整 ScriptComponent framework。
7. per actor script binding。
8. full HMR 和状态保持。
9. 复杂 async / promise 系统。
10. 大规模重构现有 V8 runtime。
11. LLM 项目的多人模式、DM 叙述、RAG、agent loop。
12. Blender / combat demo。

这些可以作为 roadmap，但不要阻碍 JS-driven Actor Demo v0。

## 请先执行的调查任务

请先阅读当前仓库代码，不要立刻大规模修改。

请重点调查：

1. V8 runtime 初始化、Isolate / Context 生命周期在哪里管理。
2. 当前 JS module 加载、执行、resolve 的实现方式。
3. 当前 V8 Inspector / DevTools 相关代码结构。
4. 当前 UE plugin 是否已有 Tickable object、Subsystem、Module Tick 或 ActorComponent tick。
5. 当前是否已有 console command 注册机制。
6. 当前是否已有 file loading、Pak / IoStore 读取逻辑。
7. 当前 JS error / stack trace 捕获方式。
8. 当前是否已有 C++ ⇄ JS value conversion。
9. 当前是否适合加入 native function binding。
10. 当前 demo UE project 中是否已有适合 spawn 的 asset / blueprint。
11. 当前 README 和目录结构如何组织 demo scripts。

## 请输出的提案内容

请基于仓库实际代码，输出一份具体实现提案。请包括：

### A. 当前代码现状总结

请说明当前项目已经具备哪些能力，相关代码大致位于哪些文件或模块。

### B. 缺口分析

请列出为了完成 JS-driven Actor Demo v0，还缺哪些能力。

### C. 推荐架构

请提出最小实现方案，包括：

* lifecycle manager 放在哪里
* manifest loader 放在哪里
* actor handle registry 放在哪里
* native UE API 如何注入 JS context
* tick 如何驱动 JS tick
* reload 如何执行
* error handling 如何保持安全
* demo script 如何组织

### D. 分阶段实施计划

请给出可执行步骤，建议分成小 PR / 小 commit 级别。

每一步请说明：

* 目标
* 涉及文件
* 新增/修改内容
* 风险点
* 如何验证

### E. 最小验收标准

请定义 JS-driven Actor Demo v0 完成的验收标准。

至少包括：

1. UE 启动后成功加载 manifest 指定 main module。
2. start 被调用。
3. JS 可以 spawn actor。
4. UE 每帧调用 JS tick(deltaTime)。
5. actor 可以在 JS 控制下移动。
6. dispose 可以清理 actor。
7. RinRinJs.Reload 可以重新加载脚本。
8. 修改 TS 参数后，不重启 UE Editor 即可看到变化。
9. Chrome DevTools 可以在 tick 中断点调试。
10. JS 错误不会导致 UE 崩溃，而是输出清晰错误。

### F. README / Demo 文档更新建议

请提出 README 应如何更新，包括：

* 项目一句话介绍
* 当前功能
* Demo 运行方法
* TypeScript watch 方法
* UE reload command
* Chrome DevTools 调试方法
* 当前限制
* Roadmap

### G. 风险与取舍

请明确哪些地方为了 demo v0 做了简化，例如：

* ActorHandle 不是完整 UObject wrapper。
* manifest v0 不是完整 package manager。
* reload 不是 full HMR。
* module-level tick 不是最终 ScriptComponent 设计。
* source map 如果暂不支持，需要说明。

## 重要设计原则

1. 优先可展示成果，不要过度设计。
2. 优先稳定，不要让 JS 错误导致 UE 崩溃。
3. 优先小步实现，不要一次性重构所有 runtime。
4. 保持未来扩展空间，但不要为了未来扩展牺牲当前 demo。
5. JS ⇄ UE bridge 第一阶段必须是白名单 API。
6. 不要做自动 UObject reflection。
7. 不要把 package manager 作为主线。
8. 代码设计要能解释给招聘方听：这是 Unreal Engine embedded JavaScript runtime with Chrome DevTools debugging and development-time script reload。
9. 最终目标是能录制一个清楚的视频 demo，而不是只完成内部架构。

## 最终展示目标

最终视频 demo 应该可以这样展示：

1. UE Editor 中 actor 正在运动。
2. 说明这个 actor 是由 JS tick 控制的。
3. 打开 VSCode，修改 TypeScript 参数，例如 radius 或 speed。
4. tsc watch 自动编译。
5. 回到 UE，执行 RinRinJs.Reload。
6. 不重启 UE Editor，actor 运动轨迹立即变化。
7. 打开 Chrome DevTools，在 tick 中打断点。
8. UE 运行时停在 JS 代码中，可以查看变量。
9. 故意制造一个 JS 错误，UE 不崩溃，Output Log 显示清晰错误。
10. 修正错误后 reload，继续运行。

这就是下一阶段的核心价值。

请先不要直接实现全部内容。请先基于当前代码现状输出具体提案和实施计划。如果发现当前代码中已经具备部分能力，请复用现有结构；如果发现当前结构不适合，请提出最小必要调整，而不是大规模重写。
