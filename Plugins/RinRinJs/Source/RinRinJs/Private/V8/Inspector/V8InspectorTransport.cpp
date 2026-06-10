// V8InspectorTransport.cpp
#include "V8InspectorTransport.h"
#include "Util/Log.h"

#include <cstring>
#include <sstream>
#include <utility>

extern "C"
{
#include "ThirdParty/civetweb/include/civetweb.h"
}

namespace rinrin::uejs::inspector
{
    FV8InspectorTransport::FV8InspectorTransport(FOptions InOptions)
        : Options(std::move(InOptions))
    {
        if (Options.Uri.empty())
        {
            Options.Uri = "/";
        }

        // 默认允许本机地址
        HttpAllowedAddrs.push_back("127.0.0.1");
        HttpAllowedAddrs.push_back("::1");
        HttpAllowedAddrs.push_back("0:0:0:0:0:0:0:1");
    }

    FV8InspectorTransport::~FV8InspectorTransport()
    {
        Stop();
    }

    bool FV8InspectorTransport::Start()
    {
        // 0) 编译期特性检查（你必须在 Build.cs 定义 USE_WEBSOCKET）
        if (!(mg_check_feature(MG_FEATURES_WEBSOCKET) & MG_FEATURES_WEBSOCKET))
        {
            UEJS_LOG(LogJsInspector, Error, "CivetWeb compiled without WebSocket support (USE_WEBSOCKET not set)");
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
            UEJS_LOG(LogJsInspector, Error,
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

        // HTTP discovery handlers (/json, /json/list, /json/version)
        mg_set_request_handler(Ctx, "/json", &FV8InspectorTransport::JsonHttpHandler, this);
        mg_set_request_handler(Ctx, "/json/list", &FV8InspectorTransport::JsonHttpHandler, this);
        mg_set_request_handler(Ctx, "/json/version", &FV8InspectorTransport::JsonHttpHandler, this);

        mg_set_websocket_handler(
            Ctx,
            Options.Uri.c_str(),
            &FV8InspectorTransport::WsConnectHandler,
            &FV8InspectorTransport::WsReadyHandler,
            &FV8InspectorTransport::WsDataHandler,
            &FV8InspectorTransport::WsCloseHandler, this);

        UEJS_LOG(LogJsInspector, Log, "CivetWeb Inspector WS listening on ws://127.0.0.1:{}{}",
                 Options.Port, Options.Uri);

        return true;
    }

    void FV8InspectorTransport::Stop()
    {
        bStopping = true;

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

    void FV8InspectorTransport::SetOnConnected(std::function<void()> Fn)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        OnConnected = std::move(Fn);
    }

    void FV8InspectorTransport::SetOnDisconnected(std::function<void()> Fn)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        OnDisconnected = std::move(Fn);
    }

    void FV8InspectorTransport::SetTargetInfo(FTargetInfo Info)
    {
        std::lock_guard<std::mutex> Lock(MetaMutex);
        TargetInfo = std::move(Info);
    }

    void FV8InspectorTransport::AddHttpAllowedAddress(std::string Addr)
    {
        std::lock_guard<std::mutex> Lock(HttpAclMutex);
        HttpAllowedAddrs.push_back(std::move(Addr));
    }

    void FV8InspectorTransport::SetHttpAllowAll(bool bAllowAll)
    {
        std::lock_guard<std::mutex> Lock(HttpAclMutex);
        bHttpAllowAll = bAllowAll;
    }

    void FV8InspectorTransport::PumpTransportOnce()
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

    void FV8InspectorTransport::DrainIncomingMessages(std::function<void(std::string &&)> Fn)
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

    void FV8InspectorTransport::SendMessage(const std::string &JsonUtf8)
    {
        mg_connection *Conn = nullptr;
        {
            std::lock_guard<std::mutex> Lock(ConnMutex);
            Conn = ActiveConn.load();
        }

        if (!Conn)
        {
            UEJS_LOG(LogJsInspector, Verbose, "No active Inspector WS connection for SendMessage");
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
            UEJS_LOG(LogJsInspector, Warning, "mg_websocket_write failed (written={})", Written);
        }
    }

    bool FV8InspectorTransport::IsRemoteLoopback(const mg_connection *Conn) const
    {
        const mg_request_info *Info = mg_get_request_info(Conn);
        if (!Info)
        {
            return false;
        }

        const char *Addr = Info->remote_addr;
        return IsLoopbackAddr(Addr);
    }

    bool FV8InspectorTransport::IsLoopbackAddr(const char *Addr) const
    {
        if (!Addr)
        {
            return false;
        }

        // IPv4 loopback / IPv6 loopback forms
        return (std::strcmp(Addr, "127.0.0.1") == 0) ||
               (std::strcmp(Addr, "::1") == 0) ||
               (std::strcmp(Addr, "0:0:0:0:0:0:0:1") == 0);
    }

    bool FV8InspectorTransport::IsRemoteAllowed(const mg_request_info *Info) const
    {
        if (!Info)
        {
            return false;
        }
        const char *Addr = Info->remote_addr;
        if (!Addr)
        {
            return false;
        }

        const bool bLoop = IsLoopbackAddr(Addr);
        {
            std::lock_guard<std::mutex> Lock(HttpAclMutex);
            if (bHttpAllowAll)
            {
                return true;
            }

            if (bLoop)
            {
                return true;
            }

            for (const std::string &Allowed : HttpAllowedAddrs)
            {
                if (Allowed == Addr)
                {
                    return true;
                }
            }
        }

        // 若仍要求仅本机则拒绝非 loopback
        if (Options.bLocalhostOnly)
        {
            return false;
        }

        return false;
    }

    int FV8InspectorTransport::WsConnectHandler(const mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FV8InspectorTransport *>(CbData);
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

    void FV8InspectorTransport::WsReadyHandler(mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FV8InspectorTransport *>(CbData);
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

    int FV8InspectorTransport::WsDataHandler(mg_connection *Conn, int Bits, char *Data, size_t DataLen, void *CbData)
    {
        const bool bStopping = static_cast<FV8InspectorTransport *>(CbData)->bStopping;
        if (bStopping)
        {
            return 0; // close
        }

        auto *Self = static_cast<FV8InspectorTransport *>(CbData);
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

    void FV8InspectorTransport::WsCloseHandler(const mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FV8InspectorTransport *>(CbData);
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

    int FV8InspectorTransport::JsonHttpHandler(mg_connection *Conn, void *CbData)
    {
        auto *Self = static_cast<FV8InspectorTransport *>(CbData);
        if (!Self)
        {
            return 0;
        }

        const mg_request_info *Info = mg_get_request_info(Conn);
        UEJS_LOG(LogJsInspector, VeryVerbose, "Received HTTP request for Inspector JSON discovery {}", Info->remote_addr);

        if (!Self->IsRemoteAllowed(Info))
        {
            mg_printf(Conn, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
            return 403;
        }

        if (!Info || !Info->request_method || std::strcmp(Info->request_method, "GET") != 0)
        {
            mg_printf(Conn, "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n");
            return 405;
        }

        const std::string Path = Info->local_uri ? Info->local_uri : "";
        std::string Body;
        if (Path == "/json" || Path == "/json/list")
        {
            Body = Self->BuildJsonListPayload();
        }
        else if (Path == "/json/version")
        {
            Body = Self->BuildJsonVersionPayload();
        }
        else
        {
            mg_printf(Conn, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
            return 404;
        }
        UEJS_LOG(LogJsInspector, VeryVerbose, "HTTP {} response: {}", Path, Body);
        mg_printf(Conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-cache\r\nContent-Length: %zu\r\n\r\n%s",
                  Body.size(), Body.c_str());

        return 200;
    }

    std::string FV8InspectorTransport::BuildWebSocketUrl() const
    {
        const std::string Host = Options.bLocalhostOnly ? "127.0.0.1" : "localhost"; // 默认使用 loopback
        std::string Uri = Options.Uri;
        if (Uri.empty())
        {
            Uri = "/";
        }
        else if (Uri.front() != '/')
        {
            Uri = "/" + Uri;
        }

        std::ostringstream Oss;
        Oss << "ws://" << Host << ":" << Options.Port << Uri;
        return Oss.str();
    }

    std::string FV8InspectorTransport::BuildDevToolsFrontendUrl() const
    {
        std::string Uri = Options.Uri;
        if (Uri.empty())
        {
            Uri = "/";
        }
        else if (Uri.front() != '/')
        {
            Uri = "/" + Uri;
        }

        std::ostringstream Oss;
        Oss << "devtools://devtools/bundled/js_app.html?ws=127.0.0.1:" << Options.Port << Uri;
        return Oss.str();
    }

    std::string FV8InspectorTransport::BuildJsonListPayload() const
    {
        FTargetInfo Info;
        {
            std::lock_guard<std::mutex> Lock(MetaMutex);
            Info = TargetInfo;
        }

        const std::string WsUrl = BuildWebSocketUrl();
        const std::string DevtoolsUrl = BuildDevToolsFrontendUrl();

        std::ostringstream Oss;
        Oss << "[{\"id\":\"" << Info.Id
            << "\",\"title\":\"" << Info.Title
            << "\",\"type\":\"" << Info.Type
            << "\",\"description\":\"" << Info.Description
            << "\",\"url\":\"" << Info.Url
            << "\",\"webSocketDebuggerUrl\":\"" << WsUrl
            << "\",\"devtoolsFrontendUrl\":\"" << DevtoolsUrl
            << "\"}]";
        return Oss.str();
    }

    std::string FV8InspectorTransport::BuildJsonVersionPayload() const
    {
        const std::string WsUrl = BuildWebSocketUrl();
        std::ostringstream Oss;
        Oss << "{"
            << "\"Browser\":\"UE-V8\","
            << "\"Protocol-Version\":\"1.3\","
            << "\"User-Agent\":\"UE\","
            << "\"webSocketDebuggerUrl\":\"" << WsUrl << "\""
            << "}";
        return Oss.str();
    }

} // namespace rinrin::uejs
