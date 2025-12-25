// V8InspectorHost.cpp
#include "V8InspectorHost.h"
#include "Web/IInspectorTransport.h"
#include "Common/LogMacros.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"

namespace rinrin::uejs
{
    // ---------- FChannel ----------
    FV8InspectorHost::FChannel::FChannel(v8::Isolate *InIsolate, IInspectorTransport *InTransport)
        : Isolate(InIsolate), Transport(InTransport)
    {
    }

    static std::string v8InspectorStringBufferToUtf8(v8::Isolate *Isolate, v8_inspector::StringBuffer *Buf)
    {
        const auto View = Buf->string();
        std::string outUtf8;
        // 兼容 8-bit / 16-bit StringView
        if (View.is8Bit())
        {
            outUtf8.assign(reinterpret_cast<const char *>(View.characters8()), View.length());
        }
        else
        {
            // 16-bit 转 UTF-8
            v8::HandleScope hs(Isolate);
            auto Str = v8::String::NewFromTwoByte(Isolate,
                                                  View.characters16(),
                                                  v8::NewStringType::kNormal,
                                                  static_cast<int>(View.length()))
                           .ToLocalChecked();
            v8::String::Utf8Value Utf8(Isolate, Str);
            outUtf8.assign(*Utf8 ? *Utf8 : "", Utf8.length());
        }
        return outUtf8;
    }
    void FV8InspectorHost::FChannel::sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message)
    {
        // Send(message.get());
        std::string outUtf8 = v8InspectorStringBufferToUtf8(Isolate, message.get());

        UEJS_LOG(LogJs, Log, TEXT("Sending protocol message to DevTools. id %d : %s"), callId, *FString(outUtf8.c_str()));
        Transport->SendMessage(outUtf8);
    }

    void FV8InspectorHost::FChannel::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message)
    {
        // Send(message.get());
        std::string outUtf8 = v8InspectorStringBufferToUtf8(Isolate, message.get());

        UEJS_LOG(LogJs, Log, TEXT("Sending protocol message to DevTools. %s"), *FString(outUtf8.c_str()));
        Transport->SendMessage(outUtf8);
    }
    void FV8InspectorHost::FChannel::Send(v8_inspector::StringBuffer *Buf)
    {
        if (!Transport || !Buf)
            return;

        const auto View = Buf->string();
        std::string OutUtf8;
        // 兼容 8-bit / 16-bit StringView
        if (View.is8Bit())
        {
            OutUtf8.assign(reinterpret_cast<const char *>(View.characters8()), View.length());
        }
        else
        {
            // 16-bit 转 UTF-8
            v8::HandleScope hs(Isolate);
            auto Str = v8::String::NewFromTwoByte(Isolate,
                                                  View.characters16(),
                                                  v8::NewStringType::kNormal,
                                                  static_cast<int>(View.length()))
                           .ToLocalChecked();
            v8::String::Utf8Value Utf8(Isolate, Str);
            OutUtf8.assign(*Utf8 ? *Utf8 : "", Utf8.length());
        }

        UEJS_LOG(LogJs, Log, TEXT("Sending protocol message to DevTools. %s"), *FString(OutUtf8.c_str()));
        Transport->SendMessage(OutUtf8);
    }

    // ---------- FClient ----------
    v8::Local<v8::Context> FV8InspectorHost::FClient::ensureDefaultContextInGroup(int contextGroupId)
    {
        return Host->DefaultContext.Get(Host->Isolate);
    }

    void FV8InspectorHost::FClient::runMessageLoopOnPause(int contextGroupId)
    {
        UEJS_LOG(LogJs, Log, TEXT("Inspector paused - entering message loop"));

        bPausedLoop.store(true);
        while (bPausedLoop.load())
        {
            // 关键：暂停时也要 PumpTransport + dispatch，否则 DevTools step/continue 不生效
            Host->TickOnce();
            FPlatformProcess::SleepNoStats(0.001f);
        }

        UEJS_LOG(LogJs, Log, TEXT("Inspector resumed - exiting message loop"));
    }

    void FV8InspectorHost::FClient::quitMessageLoopOnPause()
    {
        bPausedLoop.store(false);
    }

    double FV8InspectorHost::FClient::currentTimeMS()
    {
        return FPlatformTime::Seconds() * 1000.0;
    }

    // ---------- FV8InspectorHost ----------
    FV8InspectorHost::FV8InspectorHost(v8::Platform *Platform, v8::Isolate *InIsolate,
                                       v8::Local<v8::Context> InContext,
                                       IInspectorTransport *InTransport)
        : Platform(Platform), Isolate(InIsolate), Transport(InTransport)
    {
        DefaultContext.Reset(Isolate, InContext);

        // 创建 Client 和 Channel
        Client = std::make_unique<FClient>(this);
        Channel = std::make_unique<FChannel>(Isolate, Transport);

        // 创建 Inspector
        Inspector = v8_inspector::V8Inspector::create(Isolate, Client.get());

        // contextCreated：必须在执行任何 JS 之前
        const char *CtxName = "UE-V8";
        v8_inspector::V8ContextInfo Info(
            InContext,
            ContextGroupId,
            v8_inspector::StringView(reinterpret_cast<const uint8_t *>(CtxName), strlen(CtxName)));
        Inspector->contextCreated(Info);

        UEJS_LOG(LogJs, Log, TEXT("V8 Inspector created for context '%s'"), UTF8_TO_TCHAR(CtxName));

        // 把 transport 的 connected/disconnected 回调绑过来
        if (Transport)
        {
            Transport->SetOnConnected([this]()
                                      { this->Attach(); });
            Transport->SetOnDisconnected([this]()
                                         { this->Detach(); });
        }

        TickHandler = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FV8InspectorHost::Tick),
            0.016f); // 每 16ms 调用一次
    }

    FV8InspectorHost::~FV8InspectorHost()
    {
        UEJS_LOG(LogJs, Log, TEXT("Destroying V8 Inspector Host"));

        if (TickHandler.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickHandler);
            TickHandler.Reset();
        }

        Detach();

        // contextDestroyed
        if (Inspector)
        {
            Inspector->contextDestroyed(DefaultContext.Get(Isolate));
        }

        DefaultContext.Reset();
        Inspector.reset();
        Channel.reset();
        Client.reset();
    }

    void FV8InspectorHost::Attach()
    {
        if (!Inspector || Session)
        {
            UEJS_LOG(LogJs, Warning, TEXT("Inspector already attached or not initialized"));
            return;
        }

        UEJS_LOG(LogJs, Log, TEXT("Attaching Inspector session"));

        // 创建 Inspector Session
        // V8 Inspector::connect 参数：contextGroupId, Channel, state, clientTrustLevel, pauseState
        Session = Inspector->connect(
            ContextGroupId,
            Channel.get(),
            v8_inspector::StringView(),                       // state (empty for new session)
            v8_inspector::V8Inspector::kFullyTrusted,         // clientTrustLevel
            v8_inspector::V8Inspector::kNotWaitingForDebugger // pauseState
        );

        if (Session)
        {
            UEJS_LOG(LogJs, Log, TEXT("Inspector session attached successfully"));
        }
        else
        {
            UEJS_LOG(LogJs, Error, TEXT("Failed to attach Inspector session"));
        }
    }

    void FV8InspectorHost::Detach()
    {
        if (Session)
        {
            UEJS_LOG(LogJs, Log, TEXT("Detaching Inspector session"));
            Session.reset();
        }
    }

    void FV8InspectorHost::DispatchProtocolJson(const std::string &JsonUtf8)
    {
        if (!Session)
        {
            UEJS_LOG(LogJs, Verbose, TEXT("No Inspector session, ignoring protocol message"));
            return;
        }

        v8_inspector::StringView View(
            reinterpret_cast<const uint8_t *>(JsonUtf8.data()),
            JsonUtf8.size());

        UEJS_LOG(LogJs, Log, TEXT("Dispatch protocol message to Inspector. %s"), *FString(JsonUtf8.data()));
        Session->dispatchProtocolMessage(View);
    }

    void FV8InspectorHost::TickOnce()
    {
        if (!Transport)
            return;

        // 1. 先 Pump 网络：让新消息进入队列
        Transport->PumpTransportOnce();

        // 2. 再派发队列消息给 Inspector
        Transport->DrainIncomingMessages(
            [this](std::string &&Msg)
            {
                UEJS_LOG(LogJs, Verbose, TEXT("Dispatching CDP message to Inspector (%d bytes)"), Msg.size());
                this->DispatchProtocolJson(Msg);
            });

        // 关键：泵 V8 前台任务 + microtasks，避免 response 卡在 task queue 里
        if (Platform && Isolate)
        {
            while (v8::platform::PumpMessageLoop(Platform, Isolate))
            {
                // keep pumping until empty
            }
            Isolate->PerformMicrotaskCheckpoint();
        }
    }

    bool FV8InspectorHost::Tick(float DeltaTime)
    {
        TickOnce();
        return true;
    }

} // namespace rinrin::uejs
