#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::uejs
{
    class FV8Console
    {
    public:
        static void InjectConsole(v8::Isolate *Isolate, v8::Local<v8::Context> Context);
    };
} // namespace rinrin::uejs