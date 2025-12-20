#include "LogHelper.h"

#include "HAL/PlatformStackWalk.h"
#include "Containers/Array.h"
#include "Misc/AssertionMacros.h"


namespace rinrin::jsruntime
{
	void InitStackWalking()
	{
#if !UE_BUILD_SHIPPING
		// Windows 下通常足够；如果你想更激进可在更早时机调用
		FPlatformStackWalk::InitStackWalking();
#endif
	}

	FString CaptureStack(int32 IgnoreFrames)
	{
#if PLATFORM_WINDOWS && !UE_BUILD_SHIPPING
		// 64KB 通常足够；必要时可调大
		TArray<ANSICHAR> Buffer;
		Buffer.SetNumZeroed(64 * 1024);

		// +1：跳过 CaptureStack 自身
		FPlatformStackWalk::StackWalkAndDump(
			Buffer.GetData(),
			(SIZE_T)Buffer.Num(),
			IgnoreFrames + 1,
			/*Context=*/nullptr
		);

		return FString(ANSI_TO_TCHAR(Buffer.GetData()));
#else
		return FString();
#endif
	}

	FString MakeWhere(const ANSICHAR* File, int32 Line, const ANSICHAR* Function)
	{
#if !UE_BUILD_SHIPPING
		return FString::Printf(TEXT("%s(%d): %s"),
			ANSI_TO_TCHAR(File),
			Line,
			ANSI_TO_TCHAR(Function));
#else
		return FString();
#endif
	}

	Error MakeError(Errc Code, FString Msg,
		const ANSICHAR* File, int32 Line, const ANSICHAR* Function,
		int32 IgnoreFrames)
	{
		Error E;
		E.Code = Code;
		E.Msg = MoveTemp(Msg);
		E.Where = MakeWhere(File, Line, Function);

#if !UE_BUILD_SHIPPING
		// +2：通常再跳过 MakeError 与调用点（宏/包装层），你可按实际栈帧再微调
		E.Stack = CaptureStack(IgnoreFrames + 2);
#endif
		return E;
	}

	void ReportEnsure(const Error& E)
	{
#if !UE_BUILD_SHIPPING
		// 关键点：把 Stack 文本拼进 ensure 的消息里，
		// 不依赖 UE “是否额外打印调用栈”这个行为。
		ensureAlwaysMsgf(false, TEXT("%s\nWhere: %s\nStack:\n%s"),
			*E.Msg,
			*E.Where,
			*E.Stack);

		// 如果你还想让它走你自己的 LogCategory（可选）
		UE_LOG(LogJs, Error, TEXT("%s\nWhere: %s"), *E.Msg, *E.Where);
#else
		UE_LOG(LogJs, Error, TEXT("%s"), *E.Msg);
#endif
	}

	void ReportCheck(const Error& E)
	{
#if DO_CHECK
		// 开发期：致命中断 + 同样把 Stack 文本写入 checkf
		checkf(false, TEXT("%s\nWhere: %s\nStack:\n%s"),
			*E.Msg,
			*E.Where,
			*E.Stack);
#else
		// 非 DO_CHECK / Shipping：不崩，记录并返回错误由上层处理
		UE_LOG(LogJs, Error, TEXT("%s\nWhere: %s\nStack:\n%s"),
			*E.Msg,
			*E.Where,
			*E.Stack);
#endif
	}
} // namespace rinrin::jsruntime
