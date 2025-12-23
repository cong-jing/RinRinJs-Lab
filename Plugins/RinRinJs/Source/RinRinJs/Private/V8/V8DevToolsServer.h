// V8DevToolsServer.h
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "IWebSocketServer.h"
#include "INetworkingWebSocket.h"
#include "Common/Expected.h"
#include <string>

namespace rinrin::uejs
{
    /**
     * V8 DevTools Server - 通过 WebSocket 提供 Chrome DevTools Protocol 支持
     * 允许通过 Chrome DevTools 调试 JavaScript 代码
     */
    class FV8DevToolsServer
    {
    public:
        FV8DevToolsServer();
        ~FV8DevToolsServer();

        // 启动 Inspector WebSocket 服务器
        TExpected<void> Start(int32 Port = 9229);

        // 停止服务器
        void Stop();

        // 主线程 Tick：处理 WebSocket 事件和 CDP 消息
        bool Tick(float DeltaTime);

        // 获取待处理的消息队列（供 V8 Inspector 使用）
        TQueue<std::string> &GetIncomingMessageQueue() { return IncomingMessageQueue; }

        // 检查服务器是否运行中
        bool IsRunning() const { return WsServer.IsValid(); }

    private:
        // WebSocket 回调：客户端连接
        void OnClientConnected(INetworkingWebSocket *Socket);

        // WebSocket 回调：接收到消息
        void OnMessageReceived(void *Data, int32 Size);

        // 处理来自 DevTools 的消息（CDP 协议）
        void HandleInspectorMessage(const std::string &JsonMessage);

        // 发送消息到 DevTools
        void SendMessageToDevTools(const std::string &JsonMessage);

        // DevTools 连接建立
        void OnDevToolsConnected(INetworkingWebSocket *Connection);

        // DevTools 断开连接
        void OnDevToolsDisconnected();

    private:
        static constexpr int32 DefaultInspectorPort = 9229;
        static constexpr int32 ContextGroupId = 1;

        /** Ticker handle for Inspector */
        FTSTicker::FDelegateHandle TickerHandle;

        // WebSocket 服务器
        TUniquePtr<IWebSocketServer> WsServer;

        // 当前活动连接（目前只支持单个连接）
        TAtomic<INetworkingWebSocket *> ActiveConnection;

        // 待处理的消息队列（从 WebSocket 线程到主线程）
        TQueue<std::string> IncomingMessageQueue;
    };

} // namespace rinrin::uejs
