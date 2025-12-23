// V8DevToolsServer.cpp
#include "V8DevToolsServer.h"
#include "IWebSocketNetworkingModule.h"
#include "Modules/ModuleManager.h"
#include "Common/LogMacros.h"

namespace rinrin::uejs
{
    FV8DevToolsServer::FV8DevToolsServer()
        : ActiveConnection(nullptr)
    {
    }

    FV8DevToolsServer::~FV8DevToolsServer()
    {
        Stop();
    }

    TExpected<void> FV8DevToolsServer::Start(int32 Port)
    {
        if (WsServer.IsValid())
        {
            UEJS_LOG(LogJs, Warning, TEXT("V8 DevTools Server is already running"));
            return {};
        }

        UEJS_LOG(LogJs, Log, TEXT("Starting V8 DevTools Server on port %d"), Port);

        // 加载 WebSocketNetworking 模块
        IWebSocketNetworkingModule *WsModule = FModuleManager::LoadModulePtr<IWebSocketNetworkingModule>("WebSocketNetworking");
        if (!WsModule)
        {
            return UEJS_MAKE_ERROR(TEXT("WebSocketNetworking module not found"));
        }

        // 创建 WebSocket 服务器
        WsServer = WsModule->CreateServer();
        if (!WsServer)
        {
            return UEJS_MAKE_ERROR(TEXT("Failed to create WebSocket server"));
        }

        // 初始化服务器
        const bool bOk = WsServer->Init(
            Port,
            FWebSocketClientConnectedCallBack::CreateRaw(
                this, &FV8DevToolsServer::OnClientConnected),
            TEXT("127.0.0.1") // 只监听本地连接
        );
        if (!bOk)
        {
            WsServer.Reset();
            return UEJS_MAKE_ERROR(TEXT("Failed to initialize WebSocket server on port %d"), Port);
        }

        // 注册 Ticker 来处理 Inspector 消息
        if (!TickerHandle.IsValid())
        {
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateRaw(this, &FV8DevToolsServer::Tick),
                0.0f // 每帧都调用
            );
        }
        UEJS_LOG(LogJs, Log, TEXT("V8 DevTools Server started successfully on ws://127.0.0.1:%d"), Port);
        return {};
    }

    void FV8DevToolsServer::Stop()
    {
        // 移除 Ticker
        if (TickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
        }

        if (!WsServer.IsValid())
        {
            UEJS_LOG(LogJs, Warning, TEXT("V8 DevTools Server is not running"));
        }

        UEJS_LOG(LogJs, Log, TEXT("Stopping V8 DevTools Server"));

        // 清理活动连接（连接会在对象销毁时自动关闭）
        ActiveConnection.Store(nullptr);

        // 停止服务器
        WsServer.Reset();

        UEJS_LOG(LogJs, Log, TEXT("V8 DevTools Server stopped"));
    }

    bool FV8DevToolsServer::Tick(float DeltaTime)
    {
        if (WsServer.IsValid())
        {
            // Tick WebSocket 服务器，处理网络事件
            WsServer->Tick();
        }

        // 2. 处理收到的 CDP 消息
        std::string Msg;
        while (IncomingMessageQueue.Dequeue(Msg))
        {
            // TODO: 调用 v8_inspector::V8InspectorSession::dispatchProtocolMessage
            // v8_inspector::StringView View(
            //     reinterpret_cast<const uint8_t*>(Msg.data()),
            //     Msg.size()
            // );
            // InspectorSession->dispatchProtocolMessage(View);

            UEJS_LOG(LogJs, Verbose, TEXT("Processing CDP message: %s"),
                     UTF8_TO_TCHAR(Msg.c_str()));
        }

        return true; // 继续 Tick
    }

    void FV8DevToolsServer::OnClientConnected(INetworkingWebSocket *Socket)
    {
        if (!Socket)
        {
            UEJS_LOG(LogJs, Error, TEXT("OnClientConnected received null socket"));
            return;
        }

        UEJS_LOG(LogJs, Log, TEXT("DevTools client connected"));

        // 如果已有活动连接，记录警告
        if (INetworkingWebSocket *OldConn = ActiveConnection.Load())
        {
            UEJS_LOG(LogJs, Warning, TEXT("Replacing existing DevTools connection"));
        }

        // 保存新连接
        ActiveConnection.Store(Socket);

        Socket->SetSocketClosedCallBack(
            FWebSocketInfoCallBack::CreateRaw(
                this, &FV8DevToolsServer::OnDevToolsDisconnected));
        Socket->SetErrorCallBack(
            FWebSocketInfoCallBack::CreateRaw(
                this, &FV8DevToolsServer::OnDevToolsDisconnected));

        // 设置接收消息回调
        Socket->SetReceiveCallBack(
            FWebSocketPacketReceivedCallBack::CreateRaw(
                this, &FV8DevToolsServer::OnMessageReceived));

        // 通知 DevTools 连接已建立
        OnDevToolsConnected(Socket);
    }

    void FV8DevToolsServer::OnMessageReceived(void *Data, int32 Size)
    {
        if (!Data || Size <= 0)
        {
            UEJS_LOG(LogJs, Warning, TEXT("Received empty message from DevTools"));
            return;
        }

        // 将 UTF-8 数据转换为 std::string
        // 注意：WebSocketNetworking 的消息默认是文本模式
        std::string JsonMessage(reinterpret_cast<const char *>(Data), Size);

        UEJS_LOG(LogJs, Verbose, TEXT("Received DevTools message (%d bytes)"), Size);

        // 入队消息，后续在主线程处理
        IncomingMessageQueue.Enqueue(JsonMessage);

        // TODO: 需要在主线程/V8 线程处理消息
        // HandleInspectorMessage(JsonMessage);
    }

    void FV8DevToolsServer::HandleInspectorMessage(const std::string &JsonMessage)
    {
        // TODO: 解析 CDP (Chrome DevTools Protocol) 消息
        // TODO: 转发给 v8::inspector
        UEJS_LOG(LogJs, Verbose, TEXT("Handling inspector message: %s"),
                 UTF8_TO_TCHAR(JsonMessage.c_str()));
    }

    void FV8DevToolsServer::SendMessageToDevTools(const std::string &JsonMessage)
    {
        INetworkingWebSocket *Conn = ActiveConnection.Load();
        if (!Conn)
        {
            UEJS_LOG(LogJs, Warning, TEXT("No active DevTools connection to send message"));
            return;
        }

        // 发送 UTF-8 文本消息
        Conn->Send(
            reinterpret_cast<const uint8 *>(JsonMessage.data()),
            JsonMessage.size(),
            false // bPrependSize = false (必须为 false，否则 Chrome 收到的 payload 会被破坏)
        );

        UEJS_LOG(LogJs, Verbose, TEXT("Sent message to DevTools (%d bytes)"), JsonMessage.size());
    }

    void FV8DevToolsServer::OnDevToolsConnected(INetworkingWebSocket *Connection)
    {
        UEJS_LOG(LogJs, Log, TEXT("DevTools session established"));

        // TODO: 创建 v8_inspector::V8Inspector
        // TODO: 创建 v8_inspector::V8InspectorSession
        // TODO: 创建 Inspector Channel 用于消息转发
    }

    void FV8DevToolsServer::OnDevToolsDisconnected()
    {
        UEJS_LOG(LogJs, Log, TEXT("DevTools session closed"));

        ActiveConnection.Store(nullptr);

        // TODO: 销毁 Inspector Session
        // TODO: 清理资源
    }

} // namespace rinrin::uejs
