#include "V8Inspector.h"
#include "Util/Log.h"

DEFINE_LOG_CATEGORY(LogJsInspector)

namespace rinrin::uejs::inspector
{
    FV8Inspector::FV8Inspector()
    {
    }

    FV8Inspector::~FV8Inspector()
    {
        Shutdown();
    }

    void FV8Inspector::Start(v8::Platform *Platform, v8::Isolate *InIsolate, v8::Local<v8::Context> InContext)
    {
        if (!InspectorTransport)
        {
            FV8InspectorTransport::FOptions TransportOptions;
            InspectorTransport = std::make_unique<FV8InspectorTransport>(TransportOptions);
            if (InspectorTransport->Start())
            {
                InspectorHost = std::make_unique<FV8InspectorHost>(
                    Platform, InIsolate, InContext, InspectorTransport.get());
                InspectorHost->Start();
            }
        }
    }

    void FV8Inspector::Shutdown()
    {
        if (InspectorHost)
        {
            InspectorHost->Shutdown();
            InspectorHost.reset();
        }
        if (InspectorTransport)
        {
            InspectorTransport->Stop();
            InspectorTransport.reset();
        }
    }
}