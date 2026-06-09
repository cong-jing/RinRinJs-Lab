#pragma once

#include "CoreMinimal.h"

#include "ModuleResolver.h"
#include "Util/Log.h"
#include "V8/V8Includes.h"
#include "Value/ValueFromJs.h"
#include "Value/ValueIntoJs.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <span>
#include <utility>

namespace rinrin::uejs
{

    class FPackage
    {
    public:
        FPackage(v8::Isolate *InIsolate, v8::Local<v8::Context> InContext);
        FPackage(v8::Isolate *InIsolate, v8::Local<v8::Context> InContext, FPackageInfo Info);
        ~FPackage() { UnloadAll(); }

        void SetResolveAndLoadFunctions(
            rinrin::uejs::FResolveModuleIdFn InResolve,
            rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource)
        {
            ResolveModuleId = std::move(InResolve);
            LoadSourceByModuleId = std::move(InLoadSource);
        }

        TExpected<v8::Local<v8::Module>> LoadModule(
            std::string_view EntrySpecifier,
            rinrin::uejs::FResolveModuleIdFn InResolve,
            rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource);

        TExpected<FValueFromJs> ExecuteJsFunction(std::string_view ModuleId,
                                                  std::string_view ObjectName,
                                                  std::string_view FunctionName,
                                                  std::span<FValueIntoJs> Args);

        void UnloadAll();

    private:
        TExpected<v8::Local<v8::Module>> GetOrCompileModule(std::string_view ReferrerResolvedId,
                                                            std::string_view RequestSpecifier);

        static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
            v8::Local<v8::Context> context,
            v8::Local<v8::String> specifier,
            v8::Local<v8::FixedArray> import_assertions,
            v8::Local<v8::Module> referrer);

        void RememberResolvedId(v8::Local<v8::Module> Module, const std::string &ResolvedId);

        std::string LookupResolvedId(v8::Local<v8::Module> Module) const;

        static FPackage *GetManager(v8::Local<v8::Context> ctx, v8::Isolate *isolate);

    private:
        v8::Isolate *Isolate = nullptr;
        v8::Global<v8::Context> Context;

        FPackageInfo Info;

        rinrin::uejs::FResolveModuleIdFn ResolveModuleId;
        rinrin::uejs::FLoadSourceByModuleIdFn LoadSourceByModuleId;

        std::unordered_map<std::string, v8::Global<v8::Module>> ModuleCache;
        std::unordered_map<void *, std::string> ModuleIdByPtr;

        bool bUseContextEmbedder = true;

        static constexpr int kEmbedderSlot = 0;
        static constexpr int kIsolateSlot = 0;
    };

} // namespace rinrin::uejs
