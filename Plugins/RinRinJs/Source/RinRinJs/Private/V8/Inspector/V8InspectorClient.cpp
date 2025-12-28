// V8InspectorClient.cpp
#include "V8InspectorHost.h"
#include "V8InspectorUtil.h"
#include "Util/Log.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#include <cctype>
#include <format>

namespace rinrin::uejs::inspector
{

    // ---------- FClient ----------
    v8::Local<v8::Context> FV8InspectorHost::FClient::ensureDefaultContextInGroup(int contextGroupId)
    {
        return Host->DefaultContext.Get(Host->Isolate);
    }

    void FV8InspectorHost::FClient::runMessageLoopOnPause(int contextGroupId)
    {
        UEJS_LOG(LogJsInspector, Log, "Inspector paused - entering message loop");

        bPausedLoop.store(true);
        while (bPausedLoop.load())
        {
            // Pause loop must keep pumping inspector messages.
            Host->TickOnce();
            FPlatformProcess::SleepNoStats(0.001f);
        }

        UEJS_LOG(LogJsInspector, Log, "Inspector resumed - exiting message loop");
    }

    void FV8InspectorHost::FClient::quitMessageLoopOnPause()
    {
        bPausedLoop.store(false);
    }

    void FV8InspectorHost::FClient::consoleAPIMessage(int contextGroupId,
                                                      v8::Isolate::MessageErrorLevel level,
                                                      const v8_inspector::StringView &message,
                                                      const v8_inspector::StringView &url,
                                                      unsigned lineNumber,
                                                      unsigned columnNumber,
                                                      v8_inspector::V8StackTrace *)
    {
        const std::string MsgUtf8 = util::V8InspectorStringViewToUtf8(message);
        const std::string UrlUtf8 = util::V8InspectorStringViewToUtf8(url);
        // UEJS_LOG(LogJsInspector, Verbose, "Console message [{}:{}:{}] level {}: {}", UrlUtf8, lineNumber, columnNumber, static_cast<int>(level), MsgUtf8);
        const std::string log = std::format("[JS] [{}:{}:{}] {}", UrlUtf8, lineNumber, columnNumber, MsgUtf8);
        switch (level)
        {
        case v8::Isolate::kMessageError:
            UE_LOG(LogJsInspector, Error, TEXT("%s"), *FString(log.c_str()));
            break;
        case v8::Isolate::kMessageWarning:
            UE_LOG(LogJsInspector, Warning, TEXT("%s"), *FString(log.c_str()));
            break;
        case v8::Isolate::kMessageDebug:
            UE_LOG(LogJsInspector, Verbose, TEXT("%s"), *FString(log.c_str()));
            break;
        default:
            UE_LOG(LogJsInspector, Log, TEXT("%s"), *FString(log.c_str()));
            break;
        }
        // UE_LOG(LogJsInspector)
    }

    void FV8InspectorHost::FClient::consoleClear(int contextGroupId)
    {
        // UEJS_LOG(LogJsInspector, VeryVerbose, "Console cleared for contextGroup {}", contextGroupId);
    }

    void FV8InspectorHost::FClient::consoleTime(const v8_inspector::StringView &title)
    {
        // UEJS_LOG(LogJsInspector, VeryVerbose, "console.time('{}')", util::V8InspectorStringViewToUtf8(title));
    }

    void FV8InspectorHost::FClient::consoleTimeEnd(const v8_inspector::StringView &title)
    {
        // UEJS_LOG(LogJsInspector, VeryVerbose, "console.timeEnd('{}')", util::V8InspectorStringViewToUtf8(title));
    }

    double FV8InspectorHost::FClient::currentTimeMS()
    {
        return FPlatformTime::Seconds() * 1000.0;
    }

    std::unique_ptr<v8_inspector::StringBuffer> FV8InspectorHost::FClient::resourceNameToUrl(
        const v8_inspector::StringView &resourceName)
    {
        std::string resultNameUtf8;

        if (resourceName.is8Bit())
        {
            resultNameUtf8.assign(reinterpret_cast<const char *>(resourceName.characters8()), resourceName.length());
        }
        else
        {
            const uint16_t *data16 = resourceName.characters16();
            resultNameUtf8.reserve(resourceName.length());
            for (size_t i = 0; i < resourceName.length(); ++i)
            {
                resultNameUtf8 += static_cast<char>(data16[i]); // WARNING: ASCII/basic Latin-1 only
            }
        }

        for (char &c : resultNameUtf8)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }

        if (resultNameUtf8.rfind("file://", 0) != 0)
        {
            if (resultNameUtf8.size() >= 3 &&
                std::isalpha(static_cast<unsigned char>(resultNameUtf8[0])) && resultNameUtf8[1] == ':' && resultNameUtf8[2] == '/')
            {
                resultNameUtf8 = "file:///" + resultNameUtf8;
            }
        }

        UEJS_LOG(LogJsInspector, Verbose, "FV8InspectorHost::FClient. Resolved resource name to URL: {}", resultNameUtf8);

        return v8_inspector::StringBuffer::create(
            v8_inspector::StringView(reinterpret_cast<const uint8_t *>(resultNameUtf8.data()), resultNameUtf8.size()));
    }

} // namespace rinrin::uejs::inspector
