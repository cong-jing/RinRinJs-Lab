#include "Common/Error.h"

#include "HAL/PlatformStackWalk.h"
#include "Misc/OutputDevice.h" // FMsg
#include "Containers/Array.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::uejs
{
    FString FSourceLocation::ToString() const
    {
        if (!IsValid())
        {
            return TEXT("<unknown>");
        }
        return FString::Printf(TEXT("%s(%d): %s"),
                               ANSI_TO_TCHAR(File),
                               Line,
                               ANSI_TO_TCHAR(Function ? Function : ""));
    }

    FV8TryCatchInfo::FV8TryCatchInfo(v8::Isolate *Isolate, const v8::TryCatch &TryCatch)
    {
        if (!TryCatch.HasCaught())
        {
            return;
        }

        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        // 获取异常值
        v8::Local<v8::Value> Exception = TryCatch.Exception();
        if (!Exception.IsEmpty())
        {
            v8::String::Utf8Value ExceptionUtf8(Isolate, Exception);
            if (*ExceptionUtf8)
            {
                Message = UTF8_TO_TCHAR(*ExceptionUtf8);
            }
        }

        // 获取详细消息对象
        v8::Local<v8::Message> Msg = TryCatch.Message();
        if (!Msg.IsEmpty())
        {
            // 脚本名称
            v8::Local<v8::Value> ScriptNameVal = Msg->GetScriptResourceName();
            if (!ScriptNameVal.IsEmpty())
            {
                v8::String::Utf8Value ScriptNameUtf8(Isolate, ScriptNameVal);
                if (*ScriptNameUtf8)
                {
                    ScriptName = UTF8_TO_TCHAR(*ScriptNameUtf8);
                }
            }

            // 行号和列号
            Line = Msg->GetLineNumber(Context).FromMaybe(-1);
            Column = Msg->GetStartColumn(Context).FromMaybe(-1);

            // 源代码行
            v8::MaybeLocal<v8::String> SourceLineMaybe = Msg->GetSourceLine(Context);
            v8::Local<v8::String> SourceLineLocal;
            if (SourceLineMaybe.ToLocal(&SourceLineLocal))
            {
                v8::String::Utf8Value SourceLineUtf8(Isolate, SourceLineLocal);
                if (*SourceLineUtf8)
                {
                    SourceLine = UTF8_TO_TCHAR(*SourceLineUtf8);
                }
            }
        }

        // 堆栈跟踪
        v8::Local<v8::Value> StackVal;
        if (TryCatch.StackTrace(Context).ToLocal(&StackVal) && StackVal->IsString())
        {
            v8::String::Utf8Value StackUtf8(Isolate, StackVal);
            if (*StackUtf8)
            {
                Stack = UTF8_TO_TCHAR(*StackUtf8);
            }
        }
    }

    void FError::CaptureStack(int32 IgnoreFrames)
    {
#if UEJS_ERROR_ENABLE_STACKTRACE
        // 仅在需要时抓一次（你也可以改为每次覆盖）
        // 这里选择覆盖：方便 error 被“补充上下文”后再抓一份新的栈
        bHasStack = false;
        NativeStack.Reset();

        // 推荐：初始化符号解析（一般 InitStackWalking 是幂等的）
        FPlatformStackWalk::InitStackWalking();

        // 64KB 缓冲区通常够；你可未来做成宏/配置项
        TArray<ANSICHAR> Buffer;
        Buffer.SetNumZeroed(64 * 1024);

        // +1 跳过 CaptureStack 自身；你若还有额外 wrapper，可再加
        FPlatformStackWalk::StackWalkAndDump(
            Buffer.GetData(),
            (SIZE_T)Buffer.Num(),
            IgnoreFrames + 1,
            /*Context=*/nullptr);

        NativeStack = FString(ANSI_TO_TCHAR(Buffer.GetData()));
        bHasStack = !NativeStack.IsEmpty();
#else
        (void)IgnoreFrames;
        bHasStack = false;
        NativeStack = TEXT("");
#endif
    }

    FString FError::ToPrettyString(bool bIncludeV8, bool bIncludeNativeStack) const
    {
        TStringBuilder<4096> B;

        B.Append(Message);
        B.Append(TEXT("\n"));

        if (Location.IsValid())
        {
            B.Append(TEXT("Location: "));
            B.Append(Location.ToString());
            B.Append(TEXT("\n"));
        }

        if (bIncludeV8 && V8Info.IsSet())
        {
            B.Append(TEXT("V8:\n"));

            if (!V8Info.Message.IsEmpty())
            {
                B.Append(TEXT("  Message: "));
                B.Append(V8Info.Message);
                B.Append(TEXT("\n"));
            }

            if (!V8Info.ScriptName.IsEmpty())
            {
                B.Append(TEXT("  Script: "));
                B.Append(V8Info.ScriptName);
                B.Append(TEXT("\n"));
            }

            if (V8Info.Line >= 0 || V8Info.Column >= 0)
            {
                B.Append(TEXT("  Line/Col: "));
                B.Append(FString::Printf(TEXT("%d:%d"), V8Info.Line, V8Info.Column));
                B.Append(TEXT("\n"));
            }

            if (!V8Info.SourceLine.IsEmpty())
            {
                B.Append(TEXT("  Source: "));
                B.Append(V8Info.SourceLine);
                B.Append(TEXT("\n"));
            }

            if (!V8Info.Stack.IsEmpty())
            {
                B.Append(TEXT("  Stack:\n"));
                B.Append(V8Info.Stack);
                if (!V8Info.Stack.EndsWith(TEXT("\n")))
                {
                    B.Append(TEXT("\n"));
                }
            }
        }

        if (bIncludeNativeStack)
        {
            B.Append(TEXT("Native Stack:\n"));

#if UEJS_ERROR_ENABLE_STACKTRACE
            if (bHasStack && !NativeStack.IsEmpty())
            {
                B.Append(NativeStack);
                if (!NativeStack.EndsWith(TEXT("\n")))
                {
                    B.Append(TEXT("\n"));
                }
            }
            else
            {
                B.Append(TEXT("  <no stack captured>\n"));
            }
#else
            B.Append(TEXT("  <stack trace disabled: UEJS_ERROR_ENABLE_STACKTRACE=0>\n"));
#endif
        }

        return B.ToString();
    }

    void FError::Log(const FLogCategoryBase &Category,
                     ELogVerbosity::Type Verbosity,
                     bool bIncludeV8,
                     bool bIncludeNativeStack) const
    {
        const FString Text = ToPrettyString(bIncludeV8, bIncludeNativeStack);

        // 这里不使用 UE_LOG 宏，而是用 FMsg::Logf 以支持“运行时传入 Category”
        const ANSICHAR *File = Location.File ? Location.File : __FILE__;
        const int32 Line = Location.Line > 0 ? Location.Line : 0;

        FMsg::Logf(File, Line, Category.GetCategoryName(), Verbosity, TEXT("%s"), *Text);
    }

} // namespace rinrin::uejs
