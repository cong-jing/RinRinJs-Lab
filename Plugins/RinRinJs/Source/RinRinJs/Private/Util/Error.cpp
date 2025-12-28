#include "Util/Error.h"

#include "HAL/PlatformStackWalk.h"
#include "Misc/OutputDevice.h" // FMsg

#include <algorithm>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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
    std::string FSourceLocation::ToString() const
    {
        if (!IsValid())
        {
            return "<unknown>";
        }
        return std::format("[{}:{}]",
                           File ? File : "<unknown>",
                           Line);
    }

    std::string FErrorContextFrame::ToString() const
    {
        if (!Where.IsValid())
        {
            return What;
        }
        return std::format("{} @ {}", What, Where.ToString());
    }

    FJsStackInfo::FJsStackInfo(v8::Isolate *Isolate, const v8::TryCatch &TryCatch)
    {
        if (!TryCatch.HasCaught())
        {
            return;
        }

        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        v8::Local<v8::Value> Exception = TryCatch.Exception();
        if (!Exception.IsEmpty())
        {
            v8::String::Utf8Value ExceptionUtf8(Isolate, Exception);
            if (*ExceptionUtf8)
            {
                Message.assign(*ExceptionUtf8, ExceptionUtf8.length());
            }
        }

        v8::Local<v8::Message> Msg = TryCatch.Message();
        if (!Msg.IsEmpty())
        {
            v8::Local<v8::Value> ScriptNameVal = Msg->GetScriptResourceName();
            if (!ScriptNameVal.IsEmpty())
            {
                v8::String::Utf8Value ScriptNameUtf8(Isolate, ScriptNameVal);
                if (*ScriptNameUtf8)
                {
                    ScriptName.assign(*ScriptNameUtf8, ScriptNameUtf8.length());
                }
            }

            Line = Msg->GetLineNumber(Context).FromMaybe(-1);
            Column = Msg->GetStartColumn(Context).FromMaybe(-1);

            v8::MaybeLocal<v8::String> SourceLineMaybe = Msg->GetSourceLine(Context);
            v8::Local<v8::String> SourceLineLocal;
            if (SourceLineMaybe.ToLocal(&SourceLineLocal))
            {
                v8::String::Utf8Value SourceLineUtf8(Isolate, SourceLineLocal);
                if (*SourceLineUtf8)
                {
                    SourceLine.assign(*SourceLineUtf8, SourceLineUtf8.length());
                }
            }
        }

        v8::Local<v8::Value> StackVal;
        if (TryCatch.StackTrace(Context).ToLocal(&StackVal) && StackVal->IsString())
        {
            v8::String::Utf8Value StackUtf8(Isolate, StackVal);
            if (*StackUtf8)
            {
                Stack.assign(*StackUtf8, StackUtf8.length());
            }
        }
    }

    void FError::CaptureStack(int32 IgnoreFrames)
    {
#if UEJS_ERROR_ENABLE_STACKTRACE
        bHasStack = false;
        NativeStack.clear();

        FPlatformStackWalk::InitStackWalking();

        std::vector<ANSICHAR> Buffer;
        Buffer.resize(64 * 1024);
        std::fill(Buffer.begin(), Buffer.end(), 0);

        FPlatformStackWalk::StackWalkAndDump(
            Buffer.data(),
            static_cast<SIZE_T>(Buffer.size()),
            IgnoreFrames + 1,
            /*Context=*/nullptr);

        NativeStack = std::string(Buffer.data());
        bHasStack = !NativeStack.empty();
#else
        (void)IgnoreFrames;
        bHasStack = false;
        NativeStack.clear();
#endif
    }

    std::string FError::ToPrettyString(bool bIncludeJsStack, bool bIncludeNativeStack) const
    {
        std::ostringstream B;

        B << "Error: " << Message << "\n";

        if (Location.IsValid())
        {
            B << "    at ";
            if (Location.Function && *Location.Function)
            {
                B << Location.Function << ' ';
            }
            B << '(' << (Location.File ? Location.File : "unknown") << ':' << Location.Line << ")\n";
        }

        if (!ContextFrames.empty())
        {
            B << "\nContext Chain:\n";
            for (const auto &Frame : ContextFrames)
            {
                B << "    " << Frame.What;
                if (Frame.Where.IsValid())
                {
                    B << " at ";
                    if (Frame.Where.Function && *Frame.Where.Function)
                    {
                        B << Frame.Where.Function << ' ';
                    }
                    B << '(' << (Frame.Where.File ? Frame.Where.File : "unknown") << ':' << Frame.Where.Line << ')';
                }
                B << "\n";
            }
        }

        if (bIncludeJsStack && JsInfo.IsSet())
        {
            B << "\n--- JavaScript Stack ---\n";

            if (!JsInfo.Message.empty())
            {
                B << "    " << JsInfo.Message << "\n";
            }

            if (!JsInfo.ScriptName.empty() || JsInfo.Line >= 0 || JsInfo.Column >= 0)
            {
                B << "    at " << (JsInfo.ScriptName.empty() ? std::string("<anonymous>") : JsInfo.ScriptName);
                if (JsInfo.Line >= 0)
                {
                    B << ':' << JsInfo.Line;
                    if (JsInfo.Column >= 0)
                    {
                        B << ':' << JsInfo.Column;
                    }
                }
                B << "\n";
            }

            if (!JsInfo.SourceLine.empty())
            {
                B << "    Source: " << JsInfo.SourceLine << "\n";
            }

            if (!JsInfo.Stack.empty())
            {
                std::istringstream StackStream(JsInfo.Stack);
                std::string Line;
                while (std::getline(StackStream, Line))
                {
                    if (!Line.empty())
                    {
                        B << "        " << Line << "\n";
                    }
                }
            }
        }

        if (bIncludeNativeStack)
        {
            B << "\n--- Native Stack ---\n";

#if UEJS_ERROR_ENABLE_STACKTRACE
            if (bHasStack && !NativeStack.empty())
            {
                std::istringstream StackStream(NativeStack);
                std::string Line;
                while (std::getline(StackStream, Line))
                {
                    if (Line.empty())
                    {
                        continue;
                    }

                    if (Line.rfind("    ", 0) == 0)
                    {
                        B << "    " << Line << "\n";
                    }
                    else
                    {
                        B << "        " << Line << "\n";
                    }
                }
            }
            else
            {
                B << "    <no stack captured>\n";
            }
#else
            B << "    <stack trace disabled: UEJS_ERROR_ENABLE_STACKTRACE=0>\n";
#endif
        }

        return B.str();
    }

    void FError::Log(const FLogCategoryBase &Category,
                     ELogVerbosity::Type Verbosity,
                     bool bIncludeJsStack,
                     bool bIncludeNativeStack) const
    {
        const std::string Text = ToPrettyString(bIncludeJsStack, bIncludeNativeStack);

        const ANSICHAR *File = Location.File ? Location.File : __FILE__;
        const int32 Line = Location.Line > 0 ? Location.Line : 0;

        const FString WideText = UTF8_TO_TCHAR(Text.c_str());
        FMsg::Logf(File, Line, Category.GetCategoryName(), Verbosity, TEXT("%s"), *WideText);
    }

} // namespace rinrin::uejs
