// V8InspectorTransport.cpp
#include "V8InspectorTransport.h"
#include "IWebSocketNetworkingModule.h"
#include "Modules/ModuleManager.h"
#include "Common/LogMacros.h"

namespace rinrin::uejs
{
    FV8InspectorTransport::FV8InspectorTransport()
        : ActiveConnection(nullptr)
    {
    }

    FV8InspectorTransport::~FV8InspectorTransport()
    {
        Stop();
    }

    bool FV8InspectorTransport::Start(int32_t Port)
    {
        if (WsServer.IsValid())
        {
            UEJS_LOG(LogJs, Warning, TEXT("V8 Inspector Transport is already running"));
            return false;
        }

        UEJS_LOG(LogJs, Log, TEXT("Starting V8 Inspector Transport on port %d"), Port);

        // 加载 WebSocketNetworking 模块
        IWebSocketNetworkingModule *WsModule = FModuleManager::LoadModulePtr<IWebSocketNetworkingModule>("WebSocketNetworking");
        if (!WsModule)
        {
            UEJS_LOG(LogJs, Error, TEXT("WebSocketNetworking module not found"));
            return false;
        }

        // 创建 WebSocket 服务器
        WsServer = WsModule->CreateServer();
        if (!WsServer)
        {
            UEJS_LOG(LogJs, Error, TEXT("Failed to create WebSocket server"));
            return false;
        }

        // 初始化服务器
        const bool bOk = WsServer->Init(
            Port,
            FWebSocketClientConnectedCallBack::CreateRaw(
                this, &FV8InspectorTransport::OnClientConnected),
            TEXT("127.0.0.1") // 只监听本地连接
        );

        if (!bOk)
        {
            WsServer.Reset();
            UEJS_LOG(LogJs, Error, TEXT("Failed to initialize WebSocket server on port %d"), Port);
            return false;
        }

        UEJS_LOG(LogJs, Log, TEXT("V8 Inspector Transport started successfully on ws://127.0.0.1:%d"), Port);
        return true;
    }

    void FV8InspectorTransport::Stop()
    {
        if (!WsServer.IsValid())
        {
            UEJS_LOG(LogJs, Warning, TEXT("V8 Inspector Transport is not running"));
        }

        UEJS_LOG(LogJs, Log, TEXT("Stopping V8 Inspector Transport"));

        // 清理活动连接（连接会在对象销毁时自动关闭）
        ActiveConnection.store(nullptr);

        // 停止服务器
        WsServer.Reset();

        UEJS_LOG(LogJs, Log, TEXT("V8 Inspector Transport stopped"));
    }

    bool FV8InspectorTransport::IsRunning() const
    {
        return WsServer.IsValid();
    }

    void FV8InspectorTransport::PumpTransportOnce()
    {
        if (WsServer.IsValid())
        {
            WsServer->Tick();
        }
    }

    void FV8InspectorTransport::DrainIncomingMessages(std::function<void(std::string &&)> Fn)
    {
        if (!Fn)
            return;

        std::lock_guard<std::mutex> Lock(QueueMutex);
        while (!IncomingMessageQueue.empty())
        {
            Fn(std::move(IncomingMessageQueue.front()));
            IncomingMessageQueue.pop();
        }
    }

    void FV8InspectorTransport::SendMessage(const std::string &JsonUtf8)
    {
        INetworkingWebSocket *Conn = ActiveConnection.load();
        if (!Conn)
        {
            UEJS_LOG(LogJs, Warning, TEXT("No active Inspector connection to send message"));
            return;
        }

        // 发送 UTF-8 文本消息
        Conn->Send(
            reinterpret_cast<const uint8 *>(JsonUtf8.data()),
            JsonUtf8.size(),
            false // bPrependSize = false
        );

        UEJS_LOG(LogJs, Verbose, TEXT("Sent message to Inspector client (%d bytes)"), JsonUtf8.size());
    }

    void FV8InspectorTransport::SetOnConnected(std::function<void()> Fn)
    {
        OnConnectedCallback = std::move(Fn);
    }

    void FV8InspectorTransport::SetOnDisconnected(std::function<void()> Fn)
    {
        OnDisconnectedCallback = std::move(Fn);
    }

    void FV8InspectorTransport::OnClientConnected(INetworkingWebSocket *Socket)
    {
        if (!Socket)
        {
            UEJS_LOG(LogJs, Error, TEXT("OnClientConnected received null socket"));
            return;
        }

        UEJS_LOG(LogJs, Log, TEXT("Inspector client connected"));

        // 如果已有活动连接，记录警告
        if (INetworkingWebSocket *OldConn = ActiveConnection.load())
        {
            UEJS_LOG(LogJs, Warning, TEXT("Replacing existing Inspector connection"));
        }

        // 保存新连接
        ActiveConnection.store(Socket);

        Socket->SetSocketClosedCallBack(
            FWebSocketInfoCallBack::CreateRaw(
                this, &FV8InspectorTransport::OnDevToolsDisconnected));
        Socket->SetErrorCallBack(
            FWebSocketInfoCallBack::CreateRaw(
                this, &FV8InspectorTransport::OnDevToolsDisconnected));

        // 设置接收消息回调
        Socket->SetReceiveCallBack(
            FWebSocketPacketReceivedCallBack::CreateRaw(
                this, &FV8InspectorTransport::OnMessageReceived));

        // 通知 Inspector 连接已建立
        OnDevToolsConnected(Socket);

        // 调用 Inspector 回调
        if (OnConnectedCallback)
        {
            OnConnectedCallback();
        }
    }

    void FV8InspectorTransport::OnMessageReceived(void *Data, int32 Size)
    {
        if (!Data || Size <= 0)
        {
            UEJS_LOG(LogJs, Warning, TEXT("Received empty message from Inspector client"));
            return;
        }

        // 将 UTF-8 数据转换为 std::string
        std::string JsonMessage(reinterpret_cast<const char *>(Data), Size);

        UEJS_LOG(LogJs, Verbose, TEXT("Received Inspector message (%d bytes)"), Size);

        // 入队消息，后续在主线程处理
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
            IncomingMessageQueue.push(std::move(JsonMessage));
        }
    }

    void FV8InspectorTransport::OnDevToolsConnected(INetworkingWebSocket *Connection)
    {
        UEJS_LOG(LogJs, Log, TEXT("Inspector session established"));
    }

    void FV8InspectorTransport::OnDevToolsDisconnected()
    {
        UEJS_LOG(LogJs, Log, TEXT("Inspector session closed"));

        ActiveConnection.store(nullptr);

        // 调用 Inspector 回调
        if (OnDisconnectedCallback)
        {
            OnDisconnectedCallback();
        }
    }

} // namespace rinrin::uejs
