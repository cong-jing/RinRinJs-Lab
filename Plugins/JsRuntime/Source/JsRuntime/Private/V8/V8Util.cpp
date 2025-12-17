#include "V8/V8Util.h"
#include "JsRuntimeLogger.h"
#include "CoreMinimal.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::jsruntime {

FString  V8Util::V8ToFString(v8::Isolate* Isolate, v8::Local<v8::Value> Value)
{
	v8::String::Utf8Value Utf8(Isolate, Value);
	if (*Utf8)
	{
		return UTF8_TO_TCHAR(*Utf8);
	}
	return TEXT("<conversion failed>");
}

void V8Util::LogTryCatch(v8::Isolate* Isolate, v8::TryCatch& TryCatch, const TCHAR* Prefix)
{
	v8::HandleScope HandleScope(Isolate);

	v8::Local<v8::Message> Message = TryCatch.Message();
	v8::Local<v8::Value> Exception = TryCatch.Exception();

	FString ExceptionStr = V8ToFString(Isolate, Exception);

	// 1) 没有 message 的情况（少见，但要兜底）
	if (Message.IsEmpty())
	{
		UE_LOG(LogJs, Error, TEXT("%s exception: %s"), Prefix, *ExceptionStr);
		return;
	}

	// 2) 位置：文件名 / 行 / 列
	v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

	FString ResourceName = TEXT("<unknown>");
	v8::Local<v8::Value> ScriptName = Message->GetScriptResourceName();
	if (!ScriptName.IsEmpty())
	{
		ResourceName = V8ToFString(Isolate, ScriptName);
	}

	int LineNumber = Message->GetLineNumber(Context).FromMaybe(-1);
	int StartColumn = Message->GetStartColumn(Context).FromMaybe(-1);

	// 3) message 文本
	FString MessageStr = TEXT("<no message>");
	v8::Local<v8::String> Msg = Message->Get();
	if (!Msg.IsEmpty())
	{
		MessageStr = V8ToFString(Isolate, Msg);
	}

	UE_LOG(LogJs, Error, TEXT("%s: %s"), Prefix, *MessageStr);
	UE_LOG(LogJs, Error, TEXT("%s: exception=%s"), Prefix, *ExceptionStr);
	UE_LOG(LogJs, Error, TEXT("%s: at %s:%d:%d"), Prefix, *ResourceName, LineNumber, StartColumn);

	// 4) 源代码行（如果能拿到）
	v8::MaybeLocal<v8::String> SourceLine = Message->GetSourceLine(Context);
	v8::Local<v8::String> SourceLineLocal;
	if (SourceLine.ToLocal(&SourceLineLocal))
	{
		FString SourceLineStr = V8ToFString(Isolate, SourceLineLocal);
		UE_LOG(LogJs, Error, TEXT("%s: %s"), Prefix, *SourceLineStr);
	}

	// 5) 堆栈（stack trace）
	v8::Local<v8::Value> Stack;
	if (TryCatch.StackTrace(Context).ToLocal(&Stack) && Stack->IsString())
	{
		FString StackStr = V8ToFString(Isolate, Stack);
		UE_LOG(LogJs, Error, TEXT("%s stack:\n%s"), Prefix, *StackStr);
	}
	else
	{
		UE_LOG(LogJs, Error, TEXT("%s: <no stack trace>"), Prefix);
	}
}

} // namespace rinrin::jsruntime