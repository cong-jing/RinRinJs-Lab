// CivetWebInspectorTransport.cpp
#include "CivetWebInspectorTransport.h"

#include <cstring>
#include <utility>

extern "C"
{
#include "ThirdParty/civetweb/include/civetweb.h"
}

#include "Util/LogMacros.h"

namespace rinrin::uejs
{
    FCivetWebInspectorTransport::FCivetWebInspectorTransport(FOptions InOptions)
        : Options(std::move(InOptions))
    {
        if (Options.Uri.empty())
        {
            Options.Uri = "/";
        }
    }

    FCivetWebInspectorTransport::~FCivetWebInspectorTransport()
    {
        Stop();
    }

    bool FCivetWebInspectorTransport::Start()
    {
        // 0) 编译期特性检查（你必须在 Build.cs 定义 USE_WEBSOCKET）
        if (!(mg_check_feature(MG_FEATURES_WEBSOCKET) & MG_FEATURES_WEBSOCKET))
        {
            UEJS_LOG(LogJs, Error, "CivetWeb compiled without WebSocket support (USE_WEBSOCKET not set)");
            return false;
        }

        // 1) 初始化库：非 TLS 场景用 DEFAULT（示例也是这么做的）
        mg_init_library(MG_FEATURES_DEFAULT);

        PortStr = std::to_string(Options.Port);
        NumThreadsStr = std::to_string((Options.NumThreads > 0) ? Options.NumThreads : 1);

        // 2) 强制只监听 loopback，避免 Windows 防火墙/外网绑定带来的变量
        ListeningStr = "127.0.0.1:" + PortStr;

        // 3) 打开 error/access log（路径你可以换成 UE Saved/Logs 下的绝对路径）
        ErrorLogPath = "civetweb_error.log";
        AccessLogPath = "civetweb_access.log";

        const char *MgOptions[] = {
            "listening_ports", ListeningStr.c_str(),
            "num_threads", NumThreadsStr.c_str(),
            "error_log_file", ErrorLogPath.c_str(),
            "access_log_file", AccessLogPath.c_str(),
            nullptr};

        mg_callbacks Callbacks;
        std::memset(&Callbacks, 0, sizeof(Callbacks));

        // 4) 用 mg_start2 拿到错误信息
        mg_error_data Err{};
        char ErrText[512]{};
        Err.text = ErrText;
        Err.text_buffer_size = sizeof(ErrText);

        mg_init_data Init{};
        Init.callbacks = &Callbacks;
        Init.user_data = nullptr;
        Init.configuration_options = MgOptions;

        Ctx = mg_start2(&Init, &Err);

        if (!Ctx)
        {
            UEJS_LOG(LogJs, Error,
                     "mg_start2 failed. code={} sub={} text={} (see {})",
                     Err.code,
                     Err.code_sub,
                     Err.text ? Err.text : "",
                     ErrorLogPath);

            mg_exit_library();
            bLibraryInitialized = false;
            return false;
        }

        bLibraryInitialized = true;

        mg_set_websocket_handler(
            Ctx,
            Options.Uri.c_str(),
            &FCivetWebInspectorTransport::WsConnectHandler,
            &FCivetWebInspectorTransport::WsReadyHandler,
            &FCivetWebInspectorTransport::WsDataHandler,
            &FCivetWebInspectorTransport::WsCloseHandler,
            this);

        UEJS_LOG(LogJs, Log, "CivetWeb Inspector WS listening on ws://127.0.0.1:{}{}",
                 Options.Port, Options.Uri);
        return true;
    }

    void FCivetWebInspectorTransport::Stop()
    {
        {
            std::lock_guard<std::mutex> Lock(ConnMutex);
            ActiveConn.store(nullptr);
            bClientSlotTaken.store(false);
        }

        if (Ctx)
        {
            mg_stop(Ctx);
            Ctx = nullptr;
        }

        if (bLibraryInitialized)
        {
            mg_exit_library();
            bLibraryInitialized = false;
        }
    }

    void FCivetWebInspectorTransport::SetOnConnected(std::function<void()> Fn)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        OnConnected = std::move(Fn);
    }

    void FCivetWebInspectorTransport::SetOnDisconnected(std::function<void()> Fn)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        OnDisconnected = std::move(Fn);
    }

    void FCivetWebInspectorTransport::PumpTransportOnce()
    {
        // CivetWeb 自己有线程在 pump 网络；这里主要是把“连接/断开”转发到调用方线程触发。
        if (bPendingConnected.exchange(false))
        {
            std::function<void()> Fn;
            {
                std::lock_guard<std::mutex> Lock(CallbackMutex);
                Fn = OnConnected;
            }
            if (Fn)
            {
                Fn();
            }
        }

        if (bPendingDisconnected.exchange(false))
        {
            std::function<void()> Fn;
            {
                std::lock_guard<std::mutex> Lock(CallbackMutex);
                Fn = OnDisconnected;
            }
            if (Fn)
            {
                Fn();
            }
        }
    }

    void FCivetWebInspectorTransport::DrainIncomingMessages(std::function<void(std::string &&)> Fn)
    {
        for (;;)
        {
            std::string Msg;
            {
                std::lock_guard<std::mutex> Lock(InboundMutex);
                if (InboundQueue.empty())
                {
                    break;
                }
                Msg = std::move(InboundQueue.front());
                InboundQueue.pop();
            }

            Fn(std::move(Msg));
        }
    }

    void FCivetWebInspectorTransport::SendMessage(const std::string &JsonUtf8)
    {
        mg_connection *Conn = nullptr;
        {
            std::lock_guard<std::mutex> Lock(ConnMutex);
            Conn = ActiveConn.load();
        }

        if (!Conn)
        {
            UEJS_LOG(LogJs, Verbose, "No active Inspector WS connection for SendMessage");
            return;
        }

        // Ensure thread-safe write
        mg_lock_connection(Conn);
        const int Written = mg_websocket_write(
            Conn,
            MG_WEBSOCKET_OPCODE_TEXT,
            JsonUtf8.data(),
            JsonUtf8.size());
        mg_unlock_connection(Conn);

        if (Written <= 0)
        {
            UEJS_LOG(LogJs, Warning, "mg_websocket_write failed (written={})", Written);
        }
        else
        {
            UEJS_LOG(LogJs, Verbose, "Sent to DevTools ({} bytes): {}",
                     (int)JsonUtf8.size(), JsonUtf8);
        }
    }

    bool FCivetWebInspectorTransport::IsRemoteLoopback(const mg_connection *Conn) const
    {
        const mg_request_info *Info = mg_get_request_info(Conn);
        if (!Info)
        {
            return false;
        }

        const char *Addr = Info->remote_addr;
        if (!Addr)
        {
            return false;
        }

        // IPv4 loopback / IPv6 loopback forms
        return (std::strcmp(Addr, "127.0.0.1") == 0) ||
               (std::strcmp(Addr, "::1") == 0) ||
               (std::strcmp(Addr, "0:0:0:0:0:0:0:1") == 0);
    }

    int FCivetWebInspectorTransport::WsConnectHandler(const mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FCivetWebInspectorTransport *>(CbData);
        if (!Self)
        {
            return 1; // reject
        }

        if (Self->Options.bLocalhostOnly && !Self->IsRemoteLoopback(Conn))
        {
            return 1; // reject non-loopback
        }

        // Only allow a single DevTools client at a time
        bool Expected = false;
        if (!Self->bClientSlotTaken.compare_exchange_strong(Expected, true))
        {
            return 1; // reject
        }

        return 0; // accept
    }

    void FCivetWebInspectorTransport::WsReadyHandler(mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FCivetWebInspectorTransport *>(CbData);
        if (!Self)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> Lock(Self->ConnMutex);
            Self->ActiveConn.store(Conn);
        }

        // Notify host (on caller thread via PumpTransportOnce)
        Self->bPendingConnected.store(true);
    }

    int FCivetWebInspectorTransport::WsDataHandler(mg_connection *Conn, int Bits, char *Data, size_t DataLen, void *CbData)
    {
        auto *Self = static_cast<FCivetWebInspectorTransport *>(CbData);
        if (!Self)
        {
            return 0; // close
        }

        const int OpCode = (Bits & 0x0F);

        // Only TEXT frames are relevant for CDP JSON.
        if (OpCode != MG_WEBSOCKET_OPCODE_TEXT)
        {
            // Ignore ping/pong/binary etc; keep connection open.
            return 1;
        }

        if (!Data || DataLen == 0)
        {
            return 1;
        }

        std::string Msg(Data, DataLen);
        {
            std::lock_guard<std::mutex> Lock(Self->InboundMutex);
            Self->InboundQueue.push(std::move(Msg));
        }

        return 1; // keep open
    }

    void FCivetWebInspectorTransport::WsCloseHandler(const mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FCivetWebInspectorTransport *>(CbData);
        if (!Self)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> Lock(Self->ConnMutex);

            mg_connection *Active = Self->ActiveConn.load();
            if (Active == Conn)
            {
                Self->ActiveConn.store(nullptr);
            }

            Self->bClientSlotTaken.store(false);
        }

        // Notify host (on caller thread via PumpTransportOnce)
        Self->bPendingDisconnected.store(true);
    }

} // namespace rinrin::uejs
