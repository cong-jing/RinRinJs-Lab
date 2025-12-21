#pragma once

#include "CoreMinimal.h"
#include "Common/Error.h"
#include "Common/Expected.h"

DECLARE_LOG_CATEGORY_EXTERN(LogJs, Log, All);

// --------------------
// 基础日志宏：带文件名和行号
// --------------------

// 用法：UEJS_LOG(LogJs, Warning, "Something happened: %d", value);
// 输出示例：LogJs: Warning: [FileName.cpp:init 123] Something happened: 42
#define UEJS_LOG(Category, Verbosity, Fmt, ...) \
    UE_LOG(Category, Verbosity, TEXT("[%s:%s %d] " Fmt), ANSI_TO_TCHAR(__FILE__), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#define UEJS_RETURN_ERROR(Fmt, ...) \
    return ::rinrin::uejs::Err(::rinrin::uejs::FError(FString::Printf(TEXT(Fmt), ##__VA_ARGS__), UEJS_HERE));

#define UEJS_ENSURE_NOT_ERROR(Expected)     \
    do                                      \
    {                                       \
        auto &&__Expected = (Expected);     \
        if (UNLIKELY(!__Expected))          \
        {                                   \
            return Err(__Expected.Error()); \
        }                                   \
    } while (0)

// --------------------
// ENSURE 版本：只输出一次（走 ensure 通道），每次都返回错误对象
// --------------------

// 基础：格式化消息 -> FError -> ensureMsgf(一次) -> return Err(FError)
#define UEJS_ENSURE_ONCE_AND_RETURN_ERR(Cond, Fmt, ...)                                                             \
    do                                                                                                              \
    {                                                                                                               \
        if (LIKELY(Cond))                                                                                           \
        {                                                                                                           \
        }                                                                                                           \
        else                                                                                                        \
        {                                                                                                           \
            /* 每次都构造错误对象并返回（控制流一致） */                                                            \
            ::rinrin::uejs::FError _UejsErr(FString::Printf(TEXT(Fmt), ##__VA_ARGS__), UEJS_HERE);                  \
            /* 仅第一次才生成漂亮字符串并触发 ensure（避免 Tick 中重复开销） */                                     \
            static bool _UejsEnsuredOnce = false;                                                                   \
            if (!_UejsEnsuredOnce)                                                                                  \
            {                                                                                                       \
                _UejsEnsuredOnce = true;                                                                            \
                const FString _Pretty = _UejsErr.ToPrettyString(/*bIncludeV8=*/true, /*bIncludeNativeStack=*/true); \
                ensureMsgf(false, TEXT("%s"), *_Pretty);                                                            \
            }                                                                                                       \
            return ::rinrin::uejs::Err(MoveTemp(_UejsErr));                                                         \
        }                                                                                                           \
    } while (0)

// 如果你想让 ensure 的触发与返回错误分离（例如只在 Debug/Dev 触发），可以之后再加宏开关。
// 当前版本按你的需求：始终“只触发一次 ensure + 返回错误”。

// --------------------
// 纯 LOG 版本：只在指定 Category 下输出一次（不走 ensure），每次都返回错误对象
// --------------------

// 默认用 Error verbosity；也可按需要做一个带 Verbosity 参数的版本
#define UEJS_LOG_ONCE_AND_RETURN_ERR(Category, Fmt, ...)                                                       \
    do                                                                                                         \
    {                                                                                                          \
        ::rinrin::uejs::FError _UejsErr(FString::Printf(TEXT(Fmt), ##__VA_ARGS__), UEJS_HERE);                 \
        static bool _UejsLoggedOnce = false;                                                                   \
        if (!_UejsLoggedOnce)                                                                                  \
        {                                                                                                      \
            _UejsLoggedOnce = true;                                                                            \
            _UejsErr.Log((Category), ELogVerbosity::Error, /*bIncludeV8=*/true, /*bIncludeNativeStack=*/true); \
        }                                                                                                      \
        return ::rinrin::uejs::Err(MoveTemp(_UejsErr));                                                        \
    } while (0)

// 带 Verbosity 的版本（可选，但通常很实用）
#define UEJS_LOG_ONCE_V_AND_RETURN_ERR(Category, Verbosity, Fmt, ...)                                 \
    do                                                                                                \
    {                                                                                                 \
        ::rinrin::uejs::FError _UejsErr(FString::Printf(TEXT(Fmt), ##__VA_ARGS__), UEJS_HERE);        \
        static bool _UejsLoggedOnce = false;                                                          \
        if (!_UejsLoggedOnce)                                                                         \
        {                                                                                             \
            _UejsLoggedOnce = true;                                                                   \
            _UejsErr.Log((Category), (Verbosity), /*bIncludeV8=*/true, /*bIncludeNativeStack=*/true); \
        }                                                                                             \
        return ::rinrin::uejs::Err(MoveTemp(_UejsErr));                                               \
    } while (0)
