// Value/ValueFromJs.h
#pragma once

#include "CoreMinimal.h"
#include "Util/Expected.h"
#include "Value/ValueFromJs.h"
#include <string>
#include <memory>

#include "V8/V8Includes.h"

namespace rinrin::uejs
{
    struct FValueFromJsImpl
    {
    public:
        v8::Isolate *Isolate = nullptr;
        v8::Local<v8::Context> Context;
        v8::Local<v8::Value> Value;
    };

}