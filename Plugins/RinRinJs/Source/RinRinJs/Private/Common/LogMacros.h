#pragma once

#include "CoreMinimal.h"
#include "Common/Error.h"
#include "Common/Expected.h"

DECLARE_LOG_CATEGORY_EXTERN(LogJs, Log, All);

// --------------------
// 基础日志宏：带文件名和行�?
// --------------------

// 用法：UEJS_LOG(LogJs, Warning, TEXT("Something happened: %d"), value);
// 输出示例：LogJs: Warning: [FileName.cpp:init 123] Something happened: 42
#define UEJS_LOG(Category, Verbosity, Fmt, ...) \
    UE_LOG(Category, Verbosity, TEXT("[%s:%s %d] ") Fmt, ANSI_TO_TCHAR(__FILE__), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#define UEJS_MAKE_ERROR(Fmt, ...) \
    ::rinrin::uejs::Err(::rinrin::uejs::FError(FString::Printf(Fmt, ##__VA_ARGS__), UEJS_HERE))

#define UEJS_MAKE_ERROR_WITH_JS_STACK(JsError, Fmt, ...) \
    ::rinrin::uejs::Err(::rinrin::uejs::FError(JsError, FString::Printf(Fmt, ##__VA_ARGS__), UEJS_HERE))

#define UEJS_RETURN_IF_ERROR(What, Expr)                                  \
    do                                                                    \
    {                                                                     \
        auto _r = (Expr);                                                 \
        if (!_r)                                                          \
            return ::rinrin::uejs::Err(MoveTemp(_r).TakeError().WithContext(What, UEJS_HERE)); \
    } while (0)

#define UEJS_ASSIGN_OR_RETURN(Lhs, What, Expr)                            \
    do                                                                    \
    {                                                                     \
        auto _r = (Expr);                                                 \
        if (!_r)                                                          \
            return ::rinrin::uejs::Err(MoveTemp(_r).TakeError().WithContext(What, UEJS_HERE)); \
        Lhs = std::move(*_r);                                             \
    } while (0)