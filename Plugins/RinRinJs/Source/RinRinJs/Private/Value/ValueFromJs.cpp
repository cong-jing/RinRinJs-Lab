// Value/ValueFromJs.cpp
#include "Value/ValueFromJs.h"
#include "Value/ValueFromJsImpl.h"
#include "Util/Converter.h"
#include "Util/Log.h" // Not needed currently

#if RinRinJs_USE_V8

namespace rinrin::uejs
{
    template <typename T>
    TExpected<T> MakeTypeError(FValueFromJsImpl *Impl, const char *Message)
    {
        if (Impl && Impl->Isolate && !Impl->Context.IsEmpty())
        {
            Impl->Isolate->ThrowException(v8::Exception::TypeError(util::MakeV8String(Impl->Isolate, Message)));
        }
        return UEJS_MAKE_ERROR("{}", Message);
    }

    // FValueFromJs(std::unique_ptr<FValueFromJsImpl> &&Value) : Impl(std::move(Value)) {}
    FValueFromJs::FValueFromJs(std::unique_ptr<FValueFromJsImpl> &&Value) : Impl(std::move(Value))
    {
    }

    FValueFromJs::~FValueFromJs()
    {
        if (Impl)
        {
            Impl.reset();
        }
    };

    // FValueFromJs::FValueFromJs() = default;

    // FValueFromJs::FValueFromJs(std::unique_ptr<ValueFromJsImp> InImpl)
    //     : Impl(std::move(InImpl))
    // {
    // }

    bool FValueFromJs::IsUndefined() const { return !Impl->Value.IsEmpty() && Impl->Value->IsUndefined(); }
    bool FValueFromJs::IsNull() const { return !Impl->Value.IsEmpty() && Impl->Value->IsNull(); }
    bool FValueFromJs::IsBool() const { return !Impl->Value.IsEmpty() && Impl->Value->IsBoolean(); }
    bool FValueFromJs::IsInt32() const { return !Impl->Value.IsEmpty() && Impl->Value->IsInt32(); }
    bool FValueFromJs::IsDouble() const { return !Impl->Value.IsEmpty() && Impl->Value->IsNumber(); }
    bool FValueFromJs::IsString() const { return !Impl->Value.IsEmpty() && Impl->Value->IsString(); }

    TExpected<bool> FValueFromJs::AsBool() const
    {
        if (!IsBool())
        {
            return MakeTypeError<bool>(Impl.get(), "Expected bool");
        }
        return Impl->Value->BooleanValue(Impl->Isolate);
    }

    TExpected<int32> FValueFromJs::AsInt32() const
    {
        if (!IsInt32())
        {
            return MakeTypeError<int32>(Impl.get(), "Expected int32");
        }
        v8::Maybe<int32_t> MaybeInt = Impl->Value->Int32Value(Impl->Context);
        if (!MaybeInt.IsJust())
        {
            return MakeTypeError<int32>(Impl.get(), "Failed to convert to int32");
        }
        return static_cast<int32>(MaybeInt.FromJust());
    }

    TExpected<double> FValueFromJs::AsDouble() const
    {
        if (!IsDouble())
        {
            return MakeTypeError<double>(Impl.get(), "Expected number");
        }
        v8::Maybe<double> MaybeNum = Impl->Value->NumberValue(Impl->Context);
        if (!MaybeNum.IsJust())
        {
            return MakeTypeError<double>(Impl.get(), "Failed to convert to number");
        }
        return MaybeNum.FromJust();
    }

    TExpected<std::string> FValueFromJs::AsString() const
    {
        if (!IsString())
        {
            return MakeTypeError<std::string>(Impl.get(), "Expected string");
        }

        v8::String::Utf8Value Utf8(Impl->Isolate, Impl->Value);
        if (*Utf8 == nullptr)
        {
            return MakeTypeError<std::string>(Impl.get(), "Failed to convert to string");
        }
        return std::string(*Utf8, Utf8.length());
    }

    // TExpected<rinrin::uejs::FValueIntoJs> FValueFromJs::ToValueIntoJs() const
    // {
    //     if (!Impl)
    //     {
    //         return MakeTypeError<FValueIntoJs>(Impl.Get(), "Invalid JS value");
    //     }
    //     if (IsUndefined())
    //     {
    //         return FValueIntoJs::Undefined();
    //     }
    //     if (IsNull())
    //     {
    //         return FValueIntoJs::Null();
    //     }
    //     if (IsBool())
    //     {
    //         return FValueIntoJs::FromBool(Impl->Value->BooleanValue(Impl->Isolate));
    //     }
    //     if (IsInt32())
    //     {
    //         auto V = Impl->Value->Int32Value(Impl->Context);
    //         if (!V.IsJust())
    //         {
    //             return MakeTypeError<FValueIntoJs>(Impl.Get(), "Failed to convert to int32");
    //         }
    //         return FValueIntoJs::FromInt32(static_cast<int32>(V.FromJust()));
    //     }
    //     if (IsDouble())
    //     {
    //         auto V = Impl->Value->NumberValue(Impl->Context);
    //         if (!V.IsJust())
    //         {
    //             return MakeTypeError<FValueIntoJs>(Impl.Get(), "Failed to convert to number");
    //         }
    //         return FValueIntoJs::FromDouble(V.FromJust());
    //     }
    //     if (IsString())
    //     {
    //         v8::String::Utf8Value Utf8(Impl->Isolate, Impl->Value);
    //         if (*Utf8 == nullptr)
    //         {
    //             return MakeTypeError<FValueIntoJs>(Impl.Get(), "Failed to convert to string");
    //         }
    //         return FValueIntoJs::FromString(std::string(*Utf8, Utf8.length()));
    //     }

    //     return MakeTypeError<FValueIntoJs>(Impl.Get(), "Unsupported JS value type");
    // }

} // namespace rinrin::uejs

#endif // RinRinJs_USE_V8
