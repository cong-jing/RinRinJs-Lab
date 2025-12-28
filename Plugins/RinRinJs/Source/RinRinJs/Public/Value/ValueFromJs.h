// Value/ValueFromJs.h
#pragma once

#include "CoreMinimal.h"
#include "Util/Expected.h"
#include "Value/ValueIntoJs.h"
#include <memory>
#include <string>

namespace rinrin::uejs
{
    struct FValueFromJsImpl;

    class RINRINJS_API FValueFromJs
    {
    public:
        FValueFromJs(std::unique_ptr<FValueFromJsImpl> &&Value);
        ~FValueFromJs();

        bool IsUndefined() const;
        bool IsNull() const;
        bool IsBool() const;
        bool IsInt32() const;
        bool IsDouble() const;
        bool IsString() const;

        TExpected<bool> AsBool() const;
        TExpected<int32> AsInt32() const;
        TExpected<double> AsDouble() const;
        TExpected<std::string> AsString() const;

        // 便于与现有接口统一：转换为 FValueIntoJs
        // TExpected<rinrin::uejs::FValueIntoJs> ToValueIntoJs() const;

    private:
        // explicit FValueFromJs(TUniquePtr<FInternal, FInternalDeleter> InImpl);
        // TUniquePtr<FInternal, FInternalDeleter> Impl;
        std::unique_ptr<FValueFromJsImpl> Impl;
    };

} // namespace rinrin::uejs