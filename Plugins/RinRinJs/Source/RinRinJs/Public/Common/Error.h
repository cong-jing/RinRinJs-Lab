#pragma once

#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"

#ifndef UEJS_ERROR_ENABLE_STACKTRACE
// 默认开启：与 Shipping 无关（你可在 .Build.cs PublicDefinitions 覆盖）
#define UEJS_ERROR_ENABLE_STACKTRACE 1
#endif

#if RinRinJs_USE_V8
// 可选：由外部（V8 glue layer）填充的 TryCatch 信息
namespace v8
{
	class Isolate;
	class TryCatch;
}
#endif // RinRinJs_USE_V8

namespace rinrin::uejs
{
	// 可选：记录错误创建位置（用于打印更好定位）
	struct FSourceLocation
	{
		const ANSICHAR *File = nullptr;
		int32 Line = 0;
		const ANSICHAR *Function = nullptr;

		FString ToString() const;
		bool IsValid() const { return File != nullptr && Line > 0; }
	};

#define UEJS_HERE \
	::rinrin::uejs::FSourceLocation { __FILE__, __LINE__, __FUNCTION__ }

	struct FJsStackInfo
	{
		FString Message;	// exception message（尽量简短）
		FString Stack;		// JS stack trace（如果有）
		FString ScriptName; // 脚本/资源名（如果有）
		FString SourceLine; // 出错行内容（如果有）
		int32 Line = -1;	// 1-based 或 0-based 由你统一；这里只存原值
		int32 Column = -1;

		FJsStackInfo() = default;
#if RinRinJs_USE_V8
		FJsStackInfo(v8::Isolate *Isolate, const v8::TryCatch &TryCatch);
#endif // RinRinJs_USE_V8

		bool IsSet() const
		{
			return !Message.IsEmpty() || !Stack.IsEmpty() || !ScriptName.IsEmpty() || Line >= 0 || Column >= 0;
		}
	};

	class FError
	{
	public:
		FError() = default;

		explicit FError(FString InMessage, FSourceLocation InLocation = {})
			: Message(MoveTemp(InMessage)), Location(InLocation)
		{
#if UEJS_ERROR_ENABLE_STACKTRACE
			CaptureStack(/*IgnoreFrames=*/0);
#endif
		}

		FError(FString InMessage, FJsStackInfo InJsInfo, FSourceLocation InLocation = {})
			: Message(MoveTemp(InMessage)), JsInfo(MoveTemp(InJsInfo)), Location(InLocation)
		{
#if UEJS_ERROR_ENABLE_STACKTRACE
			CaptureStack(/*IgnoreFrames=*/0);
#endif
		}
		// 如果你希望“只在需要时才抓堆栈”，可用这个构造并传 false
		FError(FString InMessage, bool bCaptureNow, FSourceLocation InLocation = {})
			: Message(MoveTemp(InMessage)), Location(InLocation)
		{
#if UEJS_ERROR_ENABLE_STACKTRACE
			if (bCaptureNow)
			{
				CaptureStack(/*IgnoreFrames=*/0);
			}
#else
			(void)bCaptureNow;
#endif
		}

		// 基本信息
		const FString &GetMessage() const { return Message; }
		const FSourceLocation &GetLocation() const { return Location; }

		// JS 信息（可选）
		bool HasJsStack() const { return JsInfo.IsSet(); }
		const FJsStackInfo &GetJsStack() const { return JsInfo; }
		void SetJsStack(FJsStackInfo InInfo) { JsInfo = MoveTemp(InInfo); }

		// 原生堆栈
		bool HasNativeStack() const { return bHasStack; }
		const FString &GetNativeStack() const { return NativeStack; }

		// 主动抓取堆栈（IgnoreFrames：用于跳过包装层）
		void CaptureStack(int32 IgnoreFrames = 0);

		// 格式化输出（漂亮打印：包含 Message/Location/V8/Native stack）
		FString ToPrettyString(bool bIncludeJsStack = true, bool bIncludeNativeStack = true) const;

		// 打印到 UE 日志：支持运行时传入 category
		void Log(const FLogCategoryBase &Category,
				 ELogVerbosity::Type Verbosity = ELogVerbosity::Error,
				 bool bIncludeJsStack = true,
				 bool bIncludeNativeStack = true) const;

	private:
		FString Message;
		FJsStackInfo JsInfo;
		FSourceLocation Location;
		bool bHasStack = false;
		FString NativeStack;
	};

} // namespace rinrin::uejs
