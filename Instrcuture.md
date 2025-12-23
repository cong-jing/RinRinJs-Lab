# Unreal 插件工程当前状态总结（UE JS Runtime Plugin）

> 目的：用于对齐 AI / 合作开发者对该 Unreal 插件工程“已经做了什么、目前处于什么阶段、正在解决什么问题”的共同理解。

---

## 1. 项目目标与定位

- 目标是在 UE5（当前使用 UE5.7 + VS2022/MSVC 14.44.35207，C++20）中实现一个 **可嵌入的 JavaScript 运行时插件**，用于：
  - 游戏内脚本化与扩展（例如创意工坊/Mod/数据驱动逻辑/可热更新的逻辑层等）；
  - 为不同规模的内容作者提供不同的开发体验：
    - 大规模作者：希望能用 TS/类型补全/IDE 辅助；
    - 小规模作者：希望能直接写 JS，无需额外工具链，启动游戏即可验证。
- 项目重点不仅是“能跑 JS”，还包括工程化能力：生命周期管理、错误处理、调试体验、异步/协程友好封装、以及与 UE 的模块系统/打包流程的兼容。
- 以上只是目标，目前实现了什么看下面的章节。

---

## 2. 插件架构与生命周期（已明确并搭建骨架）

- 插件以 UE Plugin机制组织，入口是 RinRinGame\Plugins\RinRinJs\Source\RinRinJs\Public\RinRinJs
  - 作为测试，目前从RinRinGameInstance的`Init/Shutdown` 启动插件模块（`FRinRinJsModule`）
  - Editor 启动时模块会提前启动；
- 插件内部存在“运行时管理层”的概念（Runtime Manager / Runtime API 边界层）：
  - 负责 JS 引擎初始化/停止；
  - 负责执行脚本入口（例如 `ExecuteJavaScript` 一类 API）与参数/返回值的封装策略；
  - 并讨论过插件内部是否应尽量减少 UE 特有类型依赖（如 `FString` vs `std::string`）以及取舍理由。

---

## 3. 第三方 JS 引擎接入探索与工程风险识别（已完成调研与踩坑）

> 项目在“选型 + 实际接入”上已经形成清晰经验：轻量引擎的移植成本、V8 的集成复杂度、以及 Windows/MSVC/UBT 下的一致性问题。

- QuickJS（koush/quickjs）：
  - 在 Windows/MSVC 下出现较多兼容性问题（如 `sys/time.h`、`__attribute__((packed))` 等），判断移植成本过高，已放弃。
- V8：
  - 已经实际推进过 V8 编译与 UE 集成路线，确认其工程复杂度与坑位集中在：
    - UBT include/public/private 路径调整引发的编译异常（例如从 PrivateIncludePaths 移到 PublicIncludePaths 后触发 MSVC 头解析问题）；
    - Editor/HotReload/重启后的“模块缺失需重编译”与宏定义不一致导致的编译失败（例如 `v8config.h` 宏缺失/不匹配）；
    - CRT/链接方式（MT/MD）与 UE/第三方库的一致性；
    - GN args、编译配置、符号与调试链路维护成本较高。
  - 目前已经集成一些功能
    - 创建Isolate和Context
    - 直接执行脚本（Evaluate）
    - 读取esm模块，并解决模块依赖
  - 当前编译为静态库，lib和include文件放置于RinRinGame\Plugins\RinRinJs\ThirdParty\v8，编译时args.gn如下
```
is_component_build = false
is_debug = false
target_cpu = "x64"
target_os = "win"

# 功能裁剪 & 打包形式
v8_enable_i18n_support = false
v8_monolithic = true
v8_use_external_startup_data = false
v8_enable_pointer_compression = true
v8_jitless = false          # 启用 JIT
v8_enable_sandbox = true

# 与 MSVC 工程对齐
use_custom_libcxx = false   # 使用 MSVC STL，而不是 libc++
treat_warnings_as_errors = false
v8_symbol_level = 2         # 较小的 lib，符号少一点
strip_debug_info = false

```
  - ChakraCore：
    - 仅导入了基础的 ChakraCore 二进制（DLL）与头文件，尚未深入集成，但预期集成复杂度低于 V8。
    - 目前主要工作在实现v8，ChakraCore作为v8集成失败后的备选方案。

---

## 4. 错误处理与调试可观测性（当前重点推进并已确定技术路线）

> 这是当前阶段最关键的“工程完成度提升项”：让插件在开发期具备快速定位能力，同时为上层提供可结构化处理的错误返回。

- 已明确区分两类异常/错误来源：
  - **JS 脚本异常**：运行 JS 过程中抛出的异常（不属于本次重点）；
  - **C++/插件内部错误**：例如数据不一致、模块找不到、对象为空、类型转换不符合预期（多为“程序员不变量被破坏”）等。
- 已确定策略：
  - 开发期对“不变量失败”使用 UE 风格的 `ensureMsgf / checkf`（确保能快速暴露错误）；
  - 同时需要把错误**结构化返回**到上层（而不是只打印 log），便于调用方决定 UI/降级/中止策略；
  - 强烈需求：**日志必须能定位来源（文件名+行号）**，并且 **Error 及以上必须输出堆栈**（UE 默认 log 不带堆栈，定位效率不足）。
- 进一步的日志输出策略已明确（目标行为）：
  - **一般日志**：默认打印 `file(line):function` 前缀；可通过宏开关切换为普通 `UE_LOG` 风格；
  - **Error（含）以上日志**：自动追加堆栈文本（同样可通过宏开关控制是否启用）；
  - 错误对象向上返回时，尽量同样携带堆栈与位置，便于上层显示或上报。
---

## 6. 当前工程状态画像（完成项 vs 进行中）

- 已完成/已明确：
  - 插件 Module 生命周期与 UE 启动/Editor/打包差异的关键认知与骨架；
  - JS 引擎嵌入路线的现实工程风险识别（QuickJS 放弃、V8 集成复杂度评估、ChakraCore 作为候选）；
- 正在推进/待落地：
  - 最终确定并稳定集成目标 JS 引擎（V8 或 ChakraCore），解决宏定义/UBT/CRT/Editor 重启等一致性问题；
  - 使用FError来获取当前堆栈信息
  - 创建TExpected class来返回值或是FError


---
