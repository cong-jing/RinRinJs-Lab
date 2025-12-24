// IInspectorTransport.h
#pragma once

#include <string>
#include <functional>

namespace rinrin::uejs
{
    /**
     * Inspector Transport Interface - 定义 Inspector 传输层的抽象接口
     * 解耦 InspectorHost 和具体的传输实现（如 WebSocket）
     * 只包含 Host 实际需要的接口，生命周期管理由外部负责
     */
    class IInspectorTransport
    {
    public:
        virtual ~IInspectorTransport() = default;

        // 单次泵送传输事件（从网络层读取数据到队列）
        virtual void PumpTransportOnce() = 0;

        // 出队并处理所有待处理的消息
        // Fn 签名：void(std::string&& message)
        virtual void DrainIncomingMessages(std::function<void(std::string &&)> Fn) = 0;

        // 发送消息到调试客户端
        virtual void SendMessage(const std::string &JsonUtf8) = 0;

        // 设置连接/断开回调
        virtual void SetOnConnected(std::function<void()> Fn) = 0;
        virtual void SetOnDisconnected(std::function<void()> Fn) = 0;
    };

} // namespace rinrin::uejs
