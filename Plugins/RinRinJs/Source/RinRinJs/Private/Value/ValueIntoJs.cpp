// Value/ValueIntoJs.cpp
#include "Value/ValueIntoJs.h"
#include "Util/Converter.h"

namespace rinrin::uejs
{
    FValueIntoJs::FValueIntoJs()
        : Storage(TInPlaceType<FJsUndefinedTag>{})
    {
    }

    FValueIntoJs::FValueIntoJs(FStorage &&InStorage)
        : Storage(MoveTemp(InStorage))
    {
    }

    FValueIntoJs FValueIntoJs::Undefined()
    {
        return FValueIntoJs(FStorage(TInPlaceType<FJsUndefinedTag>{}));
    }

    FValueIntoJs FValueIntoJs::Null()
    {
        return FValueIntoJs(FStorage(TInPlaceType<FJsNullTag>{}));
    }

    FValueIntoJs FValueIntoJs::FromBool(bool b)
    {
        return FValueIntoJs(FStorage(TInPlaceType<bool>{}, b));
    }

    FValueIntoJs FValueIntoJs::FromInt32(int32 v)
    {
        return FValueIntoJs(FStorage(TInPlaceType<int32>{}, v));
    }

    FValueIntoJs FValueIntoJs::FromDouble(double v)
    {
        return FValueIntoJs(FStorage(TInPlaceType<double>{}, v));
    }

    FValueIntoJs FValueIntoJs::FromString(const std::string &s)
    {
        return FValueIntoJs(FStorage(TInPlaceType<std::string>{}, s));
    }

    FValueIntoJs FValueIntoJs::FromString(std::string &&s)
    {
        return FValueIntoJs(FStorage(TInPlaceType<std::string>{}, std::move(s)));
    }

    FValueIntoJs FValueIntoJs::FromString(const FString &s)
    {
        return FValueIntoJs(FStorage(TInPlaceType<std::string>{}, util::ToStdString(s)));
    }

    FValueIntoJs FValueIntoJs::FromString(FString &&s)
    {
        return FValueIntoJs(FStorage(TInPlaceType<std::string>{}, util::ToStdString(s)));
    }

    FValueIntoJs::EType FValueIntoJs::GetType() const
    {
        switch (Storage.GetIndex())
        {
        case 0:
            return EType::Undefined;
        case 1:
            return EType::Null;
        case 2:
            return EType::Bool;
        case 3:
            return EType::Int32;
        case 4:
            return EType::Double;
        case 5:
            return EType::String;
        default:
            return EType::Undefined;
        }
    }

    bool FValueIntoJs::IsUndefined() const { return Storage.IsType<FJsUndefinedTag>(); }
    bool FValueIntoJs::IsNull() const { return Storage.IsType<FJsNullTag>(); }
    bool FValueIntoJs::IsBool() const { return Storage.IsType<bool>(); }
    bool FValueIntoJs::IsInt32() const { return Storage.IsType<int32>(); }
    bool FValueIntoJs::IsDouble() const { return Storage.IsType<double>(); }
    bool FValueIntoJs::IsString() const { return Storage.IsType<std::string>(); }

    bool FValueIntoJs::AsBoolChecked() const
    {
        check(Storage.IsType<bool>());
        return Storage.Get<bool>();
    }

    int32 FValueIntoJs::AsInt32Checked() const
    {
        check(Storage.IsType<int32>());
        return Storage.Get<int32>();
    }

    double FValueIntoJs::AsDoubleChecked() const
    {
        check(Storage.IsType<double>());
        return Storage.Get<double>();
    }

    const std::string &FValueIntoJs::AsStringChecked() const
    {
        check(Storage.IsType<std::string>());
        return Storage.Get<std::string>();
    }
} // namespace rinrin::uejs
