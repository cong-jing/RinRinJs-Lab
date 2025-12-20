#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogJs, Log, All);

namespace rinrin::jsruntime
{
	// 你可以按项目需要扩展
	enum class Errc : uint8
	{
		InvariantViolation,
		InvalidType,
		NullObject,
		NotFound,
		InvalidArgument,
		Unknown,
	};

	struct Error
	{
		Errc    Code = Errc::Unknown;
		FString Msg;       // 主要错误信息
		FString Where;     // file(line): function（开发期）
		FString Stack;     // 人类可读堆栈（开发期）
	};

	template <typename T>
	using Result = TVariant<T, Error>;

	// 建议在模块 StartupModule 调一次（非 Shipping），提升符号解析质量
	void InitStackWalking();

	// Windows: 使用 FPlatformStackWalk::StackWalkAndDump(ANSICHAR*, SIZE_T, ...)
	FString CaptureStack(int32 IgnoreFrames = 0);

	FString MakeWhere(const ANSICHAR* File, int32 Line, const ANSICHAR* Function);

	Error MakeError(Errc Code, FString Msg,
		const ANSICHAR* File, int32 Line, const ANSICHAR* Function,
		int32 IgnoreFrames = 0);

	// 报告：把 Msg/Where/Stack 打到 ensure/check（并保证日志里一定出现 Stack 文本）
	void ReportEnsure(const Error& E);
	void ReportCheck(const Error& E);

	// ---- 推荐宏：不变量失败时（开发期）立刻获得堆栈，同时返回错误给上层 ----

	// 失败并返回：ensureAlways（不中断）
#define RRJS_ENSURE_OR_RETURN(ValueType, Cond, Code, Fmt, ...) \
		do { \
			if (LIKELY(Cond)) { } \
			else { \
				::rinrin::jsruntime::Error _E = ::rinrin::jsruntime::MakeError( \
					(Code), FString::Printf(TEXT(Fmt), ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__, /*IgnoreFrames=*/0); \
				::rinrin::jsruntime::ReportEnsure(_E); \
				return ::rinrin::jsruntime::Result<ValueType>(MoveTemp(_E)); \
			} \
		} while (0)

	// 失败并返回：开发期 checkf（致命中断），非 DO_CHECK/Shipping 回退为日志+返回
#define RRJS_CHECK_OR_RETURN(ValueType, Cond, Code, Fmt, ...) \
		do { \
			if (LIKELY(Cond)) { } \
			else { \
				::rinrin::jsruntime::Error _E = ::rinrin::jsruntime::MakeError( \
					(Code), FString::Printf(TEXT(Fmt), ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__, /*IgnoreFrames=*/0); \
				::rinrin::jsruntime::ReportCheck(_E); \
				return ::rinrin::jsruntime::Result<ValueType>(MoveTemp(_E)); \
			} \
		} while (0)
} // namespace rinrin::jsruntime
