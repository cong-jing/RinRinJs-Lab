#pragma once

#include "CoreMinimal.h"
#include "Value/ValueIntoJs.h"
#include "Value/ValueFromJs.h"
#include "Value/ValueFromJsImpl.h"
#include <string>
#include <string_view>

#include "V8/V8Includes.h"

namespace rinrin::uejs::util
{
    inline std::string ToStdString(const FString &Str)
    {
        const FTCHARToUTF8 Utf8(*Str);
        return std::string(Utf8.Get(), Utf8.Length());
    }
#if RinRinJs_USE_V8

    // Internal helper declared in ValueFromJs.cpp (friend of FValueFromJs)
    rinrin::uejs::FValueFromJs MakeValueFromV8(v8::Isolate *Isolate, v8::Local<v8::Context> Context, v8::Local<v8::Value> Value);

    inline v8::Local<v8::String> MakeV8String(
        v8::Isolate *Isolate,
        const std::string_view Str)
    {
        return v8::String::NewFromUtf8(
                   Isolate,
                   Str.data(),
                   v8::NewStringType::kNormal,
                   static_cast<int>(Str.length()))
            .ToLocalChecked();
    }

    inline v8::Local<v8::Value> ToV8LocalValue(
        v8::Isolate *Isolate,
        v8::Local<v8::Context> Context,
        const rinrin::uejs::FValueIntoJs &In)
    {
        // Context is present for future expansion (object/array creation, etc.)
        (void)Context;

        switch (In.GetType())
        {
        case rinrin::uejs::FValueIntoJs::EType::Undefined:
            return v8::Undefined(Isolate);

        case rinrin::uejs::FValueIntoJs::EType::Null:
            return v8::Null(Isolate);

        case rinrin::uejs::FValueIntoJs::EType::Bool:
            return v8::Boolean::New(Isolate, In.AsBoolChecked());

        case rinrin::uejs::FValueIntoJs::EType::Int32:
            return v8::Integer::New(Isolate, In.AsInt32Checked());

        case rinrin::uejs::FValueIntoJs::EType::Double:
            return v8::Number::New(Isolate, In.AsDoubleChecked());

        case rinrin::uejs::FValueIntoJs::EType::String:
            return MakeV8String(Isolate, In.AsStringChecked());

        default:
            // Defensive fallback
            return v8::Undefined(Isolate);
        }
    }

    inline FValueFromJs MakeValueFromV8(v8::Isolate *Isolate, v8::Local<v8::Context> Context, v8::Local<v8::Value> Value)
    {
        auto Impl = std::make_unique<FValueFromJsImpl>();
        Impl->Isolate = Isolate;
        Impl->Context = Context;
        Impl->Value = Value;
        return FValueFromJs(std::move(Impl));
    }

    // inline TExpected<rinrin::uejs::FValueIntoJs> ToValueIntoJs(const rinrin::uejs::FValueFromJs &In)
    // {
    //     return In.ToValueIntoJs();
    // }
#endif
} // namespace rinrin::uejs::util