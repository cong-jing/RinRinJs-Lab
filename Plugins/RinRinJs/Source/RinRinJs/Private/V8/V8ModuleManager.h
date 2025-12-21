#pragma once

#include "CoreMinimal.h"

#include "ModuleResolver.h"
#include "Common/LogMacros.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <span>

namespace rinrin::uejs
{

    class FV8ModuleManager
    {
    public:
        FV8ModuleManager(v8::Isolate *InIsolate, v8::Local<v8::Context> InContext);
        ~FV8ModuleManager() { UnloadAll(); }

        TExpected<v8::Local<v8::Module>> LoadModule(
            std::string_view EntrySpecifier,
            rinrin::uejs::FResolveModuleIdFn InResolve,
            rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource);

        TExpected<void> ExcuteFunction(std::string_view ModuleId,
                                       std::string_view FunctionName,
                                       std::span<v8::Local<v8::Value>> Args,
                                       v8::Local<v8::Value> &OutResult);

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

        static FV8ModuleManager *GetManager(v8::Local<v8::Context> ctx, v8::Isolate *isolate);

        static std::string ToUtf8(v8::Isolate *isolate, v8::Local<v8::String> s);

    private:
        v8::Isolate *Isolate = nullptr;
        v8::Global<v8::Context> Context;

        rinrin::uejs::FResolveModuleIdFn ResolveModuleId;
        rinrin::uejs::FLoadSourceByModuleIdFn LoadSourceByModuleId;

        std::unordered_map<std::string, v8::Global<v8::Module>> ModuleCache;
        std::unordered_map<void *, std::string> ModuleIdByPtr;

        bool bUseContextEmbedder = true;

        static constexpr int kEmbedderSlot = 0;
        static constexpr int kIsolateSlot = 0;
    };

} // namespace rinrin::uejs
