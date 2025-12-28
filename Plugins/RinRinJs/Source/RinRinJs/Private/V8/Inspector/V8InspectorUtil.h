#pragma once

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

namespace rinrin::uejs::inspector::util
{

    // Minimal StringView -> UTF-8 helper; keeps ASCII intact and falls back for 8-bit/16-bit views.
    static std::string V8InspectorStringViewToUtf8(const v8_inspector::StringView &View)
    {
        std::string Out;
        if (View.is8Bit())
        {
            Out.assign(reinterpret_cast<const char *>(View.characters8()), View.length());
        }
        else
        {
            const uint16_t *Data16 = View.characters16();
            Out.reserve(View.length());
            for (size_t I = 0; I < View.length(); ++I)
            {
                Out += static_cast<char>(Data16[I]); // Safe for ASCII/basic Latin
            }
        }
        return Out;
    }
}