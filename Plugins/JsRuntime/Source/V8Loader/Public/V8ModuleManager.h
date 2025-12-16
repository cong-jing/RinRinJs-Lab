#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

class FV8ModuleManager
{
public:
    using FResolveModuleIdFn = std::function<bool(
        std::string_view ReferrerResolvedId,
        std::string_view RequestSpecifier,
        std::string& OutResolvedModuleId,
        std::string& OutError)>;

    using FLoadSourceByModuleIdFn = std::function<bool(
        std::string_view ResolvedModuleId,
        std::string& OutSourceUtf8,
        std::string& OutError)>;

    void Setup(v8::Isolate* InIsolate,
        v8::Local<v8::Context> InContext,
        FResolveModuleIdFn InResolve,
        FLoadSourceByModuleIdFn InLoadSource);

    // 入口加载：你传入的可以是 "mod:AAA" 或 "file:..." 等“请求 specifier”
    v8::MaybeLocal<v8::Module> LoadModule(std::string_view EntrySpecifier);

    void UnloadAll();

private:

    bool GetOrCompileModule(std::string_view ReferrerResolvedId,
        std::string_view RequestSpecifier,
        v8::Local<v8::Module>& OutModule);

    static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
        v8::Local<v8::Context> context,
        v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_assertions,
        v8::Local<v8::Module> referrer);

    void RememberResolvedId(v8::Local<v8::Module> Module, const std::string& ResolvedId);

    std::string LookupResolvedId(v8::Local<v8::Module> Module) const;

    static FV8ModuleManager* GetManager(v8::Local<v8::Context> ctx, v8::Isolate* isolate);

    static std::string ToUtf8(v8::Isolate* isolate, v8::Local<v8::String> s);

    void ThrowJsError(const char* msg);

private:
    v8::Isolate* Isolate = nullptr;
    v8::Global<v8::Context> Context;

    FResolveModuleIdFn ResolveModuleId;
    FLoadSourceByModuleIdFn LoadSourceByModuleId;

    std::unordered_map<std::string, v8::Global<v8::Module>> ModuleCache;
    std::unordered_map<void*, std::string> ModuleIdByPtr;

    bool bUseContextEmbedder = true;

    static constexpr int kEmbedderSlot = 0;
    static constexpr int kIsolateSlot = 0;
};
