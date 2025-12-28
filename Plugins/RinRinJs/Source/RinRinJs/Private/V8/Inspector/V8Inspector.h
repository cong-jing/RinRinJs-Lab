#pragma once

#include "CoreMinimal.h"
#include "V8InspectorTransport.h"
#include "V8InspectorHost.h"
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

namespace rinrin::uejs::inspector
{
    /**
     * V8 Inspector Host - 集成 v8_inspector，提供 Chrome DevTools 调试功能
     * 负责管理 Inspector、Session、Channel、Client 的生命周期
     */
    class FV8Inspector
    {
    public:
        FV8Inspector();
        ~FV8Inspector();

        void Start(v8::Platform *Platform, v8::Isolate *InIsolate, v8::Local<v8::Context> InContext);
        void Shutdown();

    private:
        /** Inspector Transport (WebSocket) for debugging */
        std::unique_ptr<FV8InspectorTransport> InspectorTransport;

        /** Inspector Host for debugging */
        std::unique_ptr<FV8InspectorHost> InspectorHost;
    };
}