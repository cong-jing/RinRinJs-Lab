// V8InspectorTransport.h
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>

struct mg_context;
struct mg_connection;

namespace rinrin::uejs::inspector
{
    /**
     * CivetWeb-based Inspector transport.
     *
     * - CivetWeb 内部自带线程处理网络事件，本类只负责：
     *   - WebSocket 回调线程：把收到的 TEXT 帧 JSON 入队
     *   - 主线程（调用方）：PumpTransportOnce() 里转发连接/断开事件回调
     *   - DrainIncomingMessages() 里出队并交给 Host 派发给 v8_inspector
     *   - SendMessage() 发送 TEXT 帧 JSON 给 DevTools
     */
    class FV8InspectorTransport
    {
    public:
        struct FOptions
        {
            int Port = 9229;
            std::string Uri = "/";          // WebSocket handler URI (e.g. "/", "/ws")
            bool bLocalhostOnly = true;     // 只允许 127.0.0.1 / ::1 连接
            int NumThreads = 2;             // CivetWeb worker threads
            std::string DocumentRoot = "."; // CivetWeb要求配置document_root（即便只用ws）
        };

        explicit FV8InspectorTransport(FOptions InOptions);
        ~FV8InspectorTransport();

        // Lifecycle
        bool Start();
        void Stop();

        // IInspectorTransport
        void PumpTransportOnce();
        void DrainIncomingMessages(std::function<void(std::string &&)> Fn);
        void SendMessage(const std::string &JsonUtf8);
        void SetOnConnected(std::function<void()> Fn);
        void SetOnDisconnected(std::function<void()> Fn);

    private:
        // CivetWeb websocket callbacks (static)
        static int WsConnectHandler(const mg_connection *Conn, void *CbData);
        static void WsReadyHandler(mg_connection *Conn, void *CbData);
        static int WsDataHandler(mg_connection *Conn, int Bits, char *Data, size_t DataLen, void *CbData);
        static void WsCloseHandler(const mg_connection *Conn, void *CbData);

        bool IsRemoteLoopback(const mg_connection *Conn) const;

    private:
        FOptions Options;

        mg_context *Ctx = nullptr;
        bool bStopping = false;
        // Connection management
        std::atomic<mg_connection *> ActiveConn{nullptr};
        std::atomic<bool> bClientSlotTaken{false};

        // Inbound message queue (from CivetWeb threads -> main thread)
        std::mutex InboundMutex;
        std::queue<std::string> InboundQueue;

        // Callbacks are invoked from PumpTransportOnce() (caller thread)
        std::mutex CallbackMutex;
        std::function<void()> OnConnected;
        std::function<void()> OnDisconnected;

        std::atomic<bool> bPendingConnected{false};
        std::atomic<bool> bPendingDisconnected{false};

        // Protect send vs close (pointer validity window)
        std::mutex ConnMutex;

        // Keep option strings alive for mg_start configuration pointers
        std::string PortStr;
        std::string NumThreadsStr;

        bool bLibraryInitialized = false;

        std::string ListeningStr;
        std::string ErrorLogPath;
        std::string AccessLogPath;
    };

} // namespace rinrin::uejs
