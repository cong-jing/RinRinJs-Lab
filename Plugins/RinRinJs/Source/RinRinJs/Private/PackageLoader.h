#pragma once
#include "Value/ValueFromJs.h"
#include "Value/ValueIntoJs.h"

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <span>

namespace rinrin::uejs
{
    class IPackageLoader
    {
    public:
        virtual ~IPackageLoader() = default;
        virtual TExpected<void> LoadPackage(FPackageInfo Info) = 0;
        virtual TExpected<FValueFromJs> ExcuteFunctionInPackage(
            std::string_view PackageName,
            std::string_view ObjectName,
            std::string_view FunctionName,
            std::span<FValueIntoJs> Args) = 0;

        virtual void UnloadAll() = 0;
    };
}