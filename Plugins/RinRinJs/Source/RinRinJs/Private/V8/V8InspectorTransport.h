// V8InspectorTransport.h
#pragma once

#include "IInspectorTransport.h"
#include "CoreMinimal.h"
#include "IWebSocketServer.h"
#include "INetworkingWebSocket.h"

#include <string>
#include <atomic>
#include <queue>
#include <mutex>
#include <memory>
#include <functional>

namespace rinrin::uejs
{
    /**
     * V8 Inspector Transport - 通过 WebSocket 提供 Chrome DevTools Protocol 传输层
     * 实现 IInspectorTransport 接口，允许通过 Chrome DevTools 调试 JavaScript 代码
     */
    class FV8InspectorTransport : public IInspectorTransport
    {
    public:
        FV8InspectorTransport();
        ~FV8InspectorTransport() override;

        // IInspectorTransport 接口实现
        bool Start(int32_t Port) override;
        void Stop() override;
        bool IsRunning() const override;
        void PumpTransportOnce() override;
        void DrainIncomingMessages(std::function<void(std::string &&)> Fn) override;
        void SendMessage(const std::string &JsonUtf8) override;
        void SetOnConnected(std::function<void()> Fn) override;
        void SetOnDisconnected(std::function<void()> Fn) override;

    private:
        // WebSocket 回调：客户端连接
        void OnClientConnected(INetworkingWebSocket *Socket);

        // WebSocket 回调：接收到消息
        void OnMessageReceived(void *Data, int32 Size);

        // DevTools 连接建立
        void OnDevToolsConnected(INetworkingWebSocket *Connection);

        // DevTools 断开连接
        void OnDevToolsDisconnected();

    private:
        static constexpr int32_t DefaultInspectorPort = 9229;

        // WebSocket 服务器（仍使用 UE 的网络模块，这部分难以替换）
        TUniquePtr<IWebSocketServer> WsServer;

        // 当前活动连接（目前只支持单个连接）
        std::atomic<INetworkingWebSocket *> ActiveConnection;

        // 待处理的消息队列（从 WebSocket 线程到主线程）
        // 使用 std::queue + std::mutex 替代 TQueue
        std::queue<std::string> IncomingMessageQueue;
        mutable std::mutex QueueMutex;

        // Inspector 回调
        std::function<void()> OnConnectedCallback;
        std::function<void()> OnDisconnectedCallback;
    };

} // namespace rinrin::uejs
