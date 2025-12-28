// V8InspectorHost.h
#pragma once

#include "CoreMinimal.h"
#include "Web/IInspectorTransport.h"
#include <memory>
#include <atomic>
#include <string>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8-inspector.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::uejs
{
    /**
     * V8 Inspector Host - 集成 v8_inspector，提供 Chrome DevTools 调试功能
     * 负责管理 Inspector、Session、Channel、Client 的生命周期
     */
    class FV8InspectorHost
    {
    public:
        FV8InspectorHost(v8::Platform *Platform, v8::Isolate *InIsolate,
                         v8::Local<v8::Context> InContext,
                         IInspectorTransport *InTransport);

        ~FV8InspectorHost();

        void Start();
        void Shutdown();

        bool Tick(float DeltaTime); // 每帧调用（主线程）
        void TickOnce();            // 在暂停的消息循环中调用
        void Attach();              // DevTools WS connected
        void Detach();              // DevTools disconnected

    private:
        FTSTicker::FDelegateHandle TickHandler;
        // --- Inspector glue types ---

        /**
         * FChannel - 实现 v8_inspector::V8Inspector::Channel 接口
         * 负责把 Inspector 的响应发送给 DevTools
         */
        class FChannel final : public v8_inspector::V8Inspector::Channel
        {
        public:
            FChannel(v8::Isolate *InIsolate, IInspectorTransport *InTransport);

            void sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void flushProtocolNotifications() override {}

        private:
            v8::Isolate *Isolate{};
            IInspectorTransport *Transport{};
        };

        /**
         * FClient - 实现 v8_inspector::V8InspectorClient 接口
         * 提供 Inspector 需要的基础功能（Context、消息循环、时间）
         */
        class FClient final : public v8_inspector::V8InspectorClient
        {
        public:
            explicit FClient(FV8InspectorHost *InHost) : Host(InHost) {}

            v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId) override;
            void runMessageLoopOnPause(int contextGroupId) override;
            void quitMessageLoopOnPause() override;

        private:
            FV8InspectorHost *Host{};
            std::atomic<bool> bPausedLoop{false};
            std::unique_ptr<v8_inspector::StringBuffer> resourceNameToUrl(const v8_inspector::StringView &resourceName) override;
        };

    private:
        void DispatchProtocolJson(const std::string &JsonUtf8);

    private:
        static constexpr int32 ContextGroupId = 1;

        v8::Platform *Platform{};
        v8::Isolate *Isolate{};
        v8::Global<v8::Context> DefaultContext;

        IInspectorTransport *Transport{};

        std::unique_ptr<FClient> Client;
        std::unique_ptr<FChannel> Channel;

        std::unique_ptr<v8_inspector::V8Inspector> Inspector;
        std::unique_ptr<v8_inspector::V8InspectorSession> Session;

        std::atomic<bool> bShuttingDown{false};
    };

} // namespace rinrin::uejs
