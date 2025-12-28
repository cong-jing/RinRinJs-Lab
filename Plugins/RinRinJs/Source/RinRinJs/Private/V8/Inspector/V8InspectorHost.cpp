// V8InspectorHost.cpp
#include "V8InspectorHost.h"
#include "V8InspectorTransport.h"
#include "V8InspectorUtil.h"
#include "Util/Log.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"

namespace rinrin::uejs::inspector
{
    // ---------- FChannel ----------
    FV8InspectorHost::FChannel::FChannel(v8::Isolate *InIsolate, FV8InspectorTransport *InTransport)
        : Isolate(InIsolate), Transport(InTransport)
    {
    }

    // static std::string v8InspectorStringBufferToUtf8(v8::Isolate *Isolate, v8_inspector::StringBuffer *Buf)
    // {
    //     const auto View = Buf->string();
    //     std::string outUtf8;
    //     // 兼容 8-bit / 16-bit StringView
    //     if (View.is8Bit())
    //     {
    //         outUtf8.assign(reinterpret_cast<const char *>(View.characters8()), View.length());
    //     }
    //     else
    //     {
    //         // 16-bit 转 UTF-8
    //         v8::HandleScope hs(Isolate);
    //         auto Str = v8::String::NewFromTwoByte(Isolate,
    //                                               View.characters16(),
    //                                               v8::NewStringType::kNormal,
    //                                               static_cast<int>(View.length()))
    //                        .ToLocalChecked();
    //         v8::String::Utf8Value Utf8(Isolate, Str);
    //         outUtf8.assign(*Utf8 ? *Utf8 : "", Utf8.length());
    //     }
    //     return outUtf8;
    // }
    void FV8InspectorHost::FChannel::sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message)
    {
        // Send(message.get());
        std::string outUtf8 = util::V8InspectorStringViewToUtf8(message.get()->string());

        UEJS_LOG(LogJsInspector, VeryVerbose, "Sending protocol message to DevTools. id {} : {}", callId, outUtf8);
        Transport->SendMessage(outUtf8);
    }

    void FV8InspectorHost::FChannel::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message)
    {
        // Send(message.get());
        std::string outUtf8 = util::V8InspectorStringViewToUtf8(message.get()->string());

        UEJS_LOG(LogJsInspector, VeryVerbose, "Sending notification message to DevTools. {}", outUtf8);
        Transport->SendMessage(outUtf8);
    }

    // ---------- FV8InspectorHost ----------
    FV8InspectorHost::FV8InspectorHost(v8::Platform *Platform, v8::Isolate *InIsolate,
                                       v8::Local<v8::Context> InContext,
                                       FV8InspectorTransport *InTransport)
        : Platform(Platform), Isolate(InIsolate), Transport(InTransport)
    {
        DefaultContext.Reset(Isolate, InContext);
    }

    FV8InspectorHost::~FV8InspectorHost()
    {
        Shutdown();
    }
    void FV8InspectorHost::Start()
    {
        UEJS_LOG(LogJsInspector, Log, "Starting V8 Inspector Host");

        // 创建 Client 和 Channel
        Client = std::make_unique<FClient>(this);
        Channel = std::make_unique<FChannel>(Isolate, Transport);

        // 创建 Inspector
        Inspector = v8_inspector::V8Inspector::create(Isolate, Client.get());

        // contextCreated：必须在执行任何 JS 之前
        const char *CtxName = "UE-V8";
        v8_inspector::V8ContextInfo Info(
            DefaultContext.Get(Isolate),
            ContextGroupId,
            v8_inspector::StringView(reinterpret_cast<const uint8_t *>(CtxName), strlen(CtxName)));
        Inspector->contextCreated(Info);

        UEJS_LOG(LogJsInspector, Log, "V8 Inspector created for context '{}'", CtxName);

        // 把 transport 的 connected/disconnected 回调绑过来
        if (Transport)
        {
            Transport->SetOnConnected([this]()
                                      { this->Attach(); });
            Transport->SetOnDisconnected([this]()
                                         { this->Detach(); });
        }

        TickHandler = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FV8InspectorHost::Tick));
    }

    void FV8InspectorHost::Shutdown()
    {
        if (bShuttingDown.exchange(true))
        {
            // 已经在关闭过程中
            return;
        }

        UEJS_LOG(LogJsInspector, Log, "Destroying V8 Inspector Host");

        // 先停止 Tick（防止 Shutdown 过程中再进 TickOnce）
        if (TickHandler.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickHandler);
            TickHandler.Reset();
        }

        // 如果你实现了 runMessageLoopOnPause，务必保证这里能让它退出
        if (Client)
        {
            Client->quitMessageLoopOnPause(); // 你自己实现：把 bPausedLoop=false
        }

        UEJS_LOG(LogJsInspector, Log, "Destroying V8 Inspector Host 2222");
        if (Isolate)
        {
            v8::Locker locker(Isolate);
            v8::Isolate::Scope isolate_scope(Isolate);
            v8::HandleScope handle_scope(Isolate);

            UEJS_LOG(LogJsInspector, Log, "Destroying V8 Inspector Host 3333");
            if (Session)
            {
                Session->stop(); // 进入 shutdown mode
                Session.reset();
            }
            if (!DefaultContext.IsEmpty())
            {
                Inspector->contextDestroyed(DefaultContext.Get(Isolate));
            }
        }
        else
        {
            UEJS_LOG(LogJsInspector, Log, "Destroying V8 Inspector Host 4444");
            Session.reset();
        }
        UEJS_LOG(LogJsInspector, Log, "Destroying V8 Inspector Host 5555");

        DefaultContext.Reset();
        Inspector.reset();
        Channel.reset();
        Client.reset();
    }

    void FV8InspectorHost::Attach()
    {
        if (!Inspector || Session)
        {
            UEJS_LOG(LogJsInspector, Warning, "Inspector already attached or not initialized");
            return;
        }

        UEJS_LOG(LogJsInspector, Log, "Attaching Inspector session");

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
            UEJS_LOG(LogJsInspector, Log, "Inspector session attached successfully");
        }
        else
        {
            UEJS_LOG(LogJsInspector, Error, "Failed to attach Inspector session");
        }
    }

    void FV8InspectorHost::Detach()
    {
        if (Session)
        {
            UEJS_LOG(LogJsInspector, Log, "Detaching Inspector session");
            Session.reset();
        }
    }

    void FV8InspectorHost::DispatchProtocolJson(const std::string &JsonUtf8)
    {
        if (!Session)
        {
            UEJS_LOG(LogJsInspector, Verbose, "No Inspector session, ignoring protocol message");
            return;
        }

        v8_inspector::StringView View(
            reinterpret_cast<const uint8_t *>(JsonUtf8.data()),
            JsonUtf8.size());

        UEJS_LOG(LogJsInspector, VeryVerbose, "Dispatch protocol message to Inspector. {}", JsonUtf8);
        Session->dispatchProtocolMessage(View);
    }

    void FV8InspectorHost::TickOnce()
    {
        if (bShuttingDown.load())
            return;

        if (!Transport)
            return;

        // 1. 先 Pump 网络：让新消息进入队列
        Transport->PumpTransportOnce();

        // 2. 再派发队列消息给 Inspector
        Transport->DrainIncomingMessages(
            [this](std::string &&Msg)
            {
                if (bShuttingDown.load())
                    return;
                UEJS_LOG(LogJsInspector, VeryVerbose, "Dispatching CDP message to Inspector ({} bytes)", Msg.size());
                this->DispatchProtocolJson(Msg);
            });

        // 关键：泵 V8 前台任务 + microtasks，避免 response 卡在 task queue 里
        if (Platform && Isolate)
        {
            while (!bShuttingDown.load() && v8::platform::PumpMessageLoop(Platform, Isolate))
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
