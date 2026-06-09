#pragma once

#include "CoreMinimal.h"
#include "Util/Error.h"
#include "Util/Expected.h"
#include <string>
#include <string_view>
#include <format>
#include <utility>

DECLARE_LOG_CATEGORY_EXTERN(LogJs, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogJsInspector, Log, All);

namespace rinrin::uejs::util
{
    // 1) 无额外参数：直接返回
    inline std::string FormatString(std::string_view fmt)
    {
        return std::string(fmt);
    }

    // 2) 有额外参数 + 编译期格式串：std::format（带编译期检查）
    template <class... Args>
        requires(sizeof...(Args) > 0)
    inline std::string FormatString(std::format_string<Args...> fmt, Args &&...args)
    {
        return std::format(fmt, std::forward<Args>(args)...);
    }
}
// 用法：UEJS_LOG(LogJs, Warning, "Something happened: %d", value);
// 输出示例：LogJs: Warning: [FileName.cpp:init 123] Something happened: 42
#define UEJS_LOG(Category, Verbosity, ...)                                        \
    do                                                                            \
    {                                                                             \
        const std::string _msg = ::rinrin::uejs::util::FormatString(__VA_ARGS__); \
        const std::string _msg2 = ::rinrin::uejs::util::FormatString(             \
            "[{}:{}] {}", __FILE__, __LINE__, _msg);                              \
        UE_LOG(Category, Verbosity, TEXT("%s"), UTF8_TO_TCHAR(_msg2.c_str()));    \
    } while (0)

// #define UEJS_LOG(Category, Verbosity, Fmt, ...) \
//     UE_LOG(Category, Verbosity, TEXT("[%s line: %d. ] ") Fmt, ANSI_TO_TCHAR(__FILE__), __LINE__, ##__VA_ARGS__)

#define UEJS_MAKE_ERROR(...)                    \
    ::rinrin::uejs::Err(::rinrin::uejs::FError( \
        ::rinrin::uejs::util::FormatString(__VA_ARGS__), UEJS_HERE));

// #define UEJS_MAKE_ERROR(Fmt, ...) \
//     ::rinrin::uejs::Err(::rinrin::uejs::FError(FString::Printf(Fmt, ##__VA_ARGS__), UEJS_HERE))

// #define UEJS_MAKE_ERROR_WITH_JS_STACK(JsError, ...) \
//     ::rinrin::uejs::Err(::rinrin::uejs::FError(JsError, ::rinrin::uejs::util::FormatString(__VA_ARGS__), UEJS_HERE));

#define UEJS_MAKE_ERROR_WITH_JS_STACK(JsStackInfo, ...) \
    ::rinrin::uejs::Err(::rinrin::uejs::FError(     \
        JsStackInfo, ::rinrin::uejs::util::FormatString(__VA_ARGS__), UEJS_HERE));

// #define UEJS_MAKE_ERROR_WITH_JS_STACK(JsError, Fmt, ...) \
//     ::rinrin::uejs::Err(::rinrin::uejs::FError(JsError, FString::Printf(Fmt, ##__VA_ARGS__), UEJS_HERE))

#define UEJS_RETURN_IF_ERROR(What, Expr)                                                       \
    do                                                                                         \
    {                                                                                          \
        auto _r = (Expr);                                                                      \
        if (!_r)                                                                               \
            return ::rinrin::uejs::Err(MoveTemp(_r).TakeError().WithContext(What, UEJS_HERE)); \
    } while (0)

#define UEJS_ASSIGN_OR_RETURN(Lhs, What, Expr)                                                 \
    do                                                                                         \
    {                                                                                          \
        auto _r = (Expr);                                                                      \
        if (!_r)                                                                               \
            return ::rinrin::uejs::Err(MoveTemp(_r).TakeError().WithContext(What, UEJS_HERE)); \
        Lhs = std::move(*_r);                                                                  \
    } while (0)