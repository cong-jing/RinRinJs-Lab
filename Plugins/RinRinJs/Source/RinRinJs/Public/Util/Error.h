#pragma once

#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include <string>
#include <utility>
#include <vector>

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

		FSourceLocation() = default;
		FSourceLocation(const ANSICHAR *InFile, int32 InLine, const ANSICHAR *InFunction)
			: File(InFile), Line(InLine), Function(InFunction) {}

		std::string ToString() const;
		bool IsValid() const { return File != nullptr && Line > 0; }
	};

#define UEJS_HERE \
	::rinrin::uejs::FSourceLocation(__FILE__, __LINE__, __FUNCTION__)

	// Context frame for logical error propagation (without re-capturing stack trace)
	struct FErrorContextFrame
	{
		std::string What;	   // Semantic description (e.g., "While resolving module X")
		FSourceLocation Where; // Optional: location where context was added

		std::string ToString() const;
	};

	struct FJsStackInfo
	{
		std::string Message;	// exception message（尽量简短）
		std::string Stack;		// JS stack trace（如果有）
		std::string ScriptName; // 脚本/资源名（如果有）
		std::string SourceLine; // 出错行内容（如果有）
		int32 Line = -1;		// 1-based 或 0-based 由你统一；这里只存原值
		int32 Column = -1;

		FJsStackInfo() = default;
#if RinRinJs_USE_V8
		FJsStackInfo(v8::Isolate *Isolate, const v8::TryCatch &TryCatch);
#endif // RinRinJs_USE_V8

		bool IsSet() const
		{
			return !Message.empty() || !Stack.empty() || !ScriptName.empty() || Line >= 0 || Column >= 0;
		}
	};

	class RINRINJS_API FError
	{
	public:
		FError() = default;

		explicit FError(std::string InMessage, FSourceLocation InLocation = {}, bool bCaptureNow = false)
			: Message(std::move(InMessage)), Location(InLocation)
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

		FError(FJsStackInfo InJsInfo, std::string InMessage, FSourceLocation InLocation = {}, bool bCaptureNow = false)
			: Message(std::move(InMessage)), JsInfo(std::move(InJsInfo)), Location(InLocation)
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
		const std::string &GetMessage() const { return Message; }
		const FSourceLocation &GetLocation() const { return Location; }

		// JS 信息（可选）
		bool HasJsStack() const { return JsInfo.IsSet(); }
		const FJsStackInfo &GetJsStack() const { return JsInfo; }
		void SetJsStack(FJsStackInfo InInfo) { JsInfo = std::move(InInfo); }

		// 原生堆栈
		bool HasNativeStack() const { return bHasStack; }
		const std::string &GetNativeStack() const { return NativeStack; }

		// 逻辑上下文（用于错误传播时追加语义信息，不重复捕获堆栈）
		// 左值版本
		FError &WithContext(std::string InWhat, FSourceLocation InWhere = {}) &
		{
			ContextFrames.push_back({std::move(InWhat), InWhere});
			return *this;
		}

		// 右值版本（关键）
		FError &&WithContext(std::string InWhat, FSourceLocation InWhere = {}) &&
		{
			ContextFrames.push_back({std::move(InWhat), InWhere});
			return std::move(*this);
		}

		const std::vector<FErrorContextFrame> &GetContextFrames() const { return ContextFrames; }

		// 主动抓取堆栈（IgnoreFrames：用于跳过包装层）
		void CaptureStack(int32 IgnoreFrames = 0);

		// 格式化输出（漂亮打印：包含 Message/Location/V8/Native stack）
		std::string ToPrettyString(bool bIncludeJsStack = true, bool bIncludeNativeStack = true) const;

		// 打印到 UE 日志：支持运行时传入 category
		void Log(const FLogCategoryBase &Category,
				 ELogVerbosity::Type Verbosity = ELogVerbosity::Error,
				 bool bIncludeJsStack = true,
				 bool bIncludeNativeStack = true) const;

	private:
		std::string Message;
		FJsStackInfo JsInfo;
		FSourceLocation Location;
		std::vector<FErrorContextFrame> ContextFrames;
		bool bHasStack = false;
		std::string NativeStack;
	};

} // namespace rinrin::uejs
