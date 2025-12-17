#include "V8/V8ModuleManager.h"
#include "V8/V8Util.h"
#include "JsRuntimeLogger.h"

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

namespace rinrin::jsruntime {

FV8ModuleManager::FV8ModuleManager(
    v8::Isolate* InIsolate,
    v8::Local<v8::Context> InContext)
    : Isolate(InIsolate)
{
    Context.Reset(Isolate, InContext);
    if (InContext->GetNumberOfEmbedderDataFields() > kEmbedderSlot)
    {
        InContext->SetAlignedPointerInEmbedderData(kEmbedderSlot, this);
        bUseContextEmbedder = true;
    }
    else
    {
        Isolate->SetData(kIsolateSlot, this);
        bUseContextEmbedder = false;
    }
}

v8::MaybeLocal<v8::Module> FV8ModuleManager::LoadModule(
    std::string_view EntrySpecifier,
    FResolveModuleIdFn InResolve,
    FLoadSourceByModuleIdFn InLoadSource)
{
    UE_LOG(LogJs, Verbose, TEXT("LoadModule: entry='%s'"), *FString(EntrySpecifier.data()));

    if (!Isolate || Context.IsEmpty())
        return v8::MaybeLocal<v8::Module>();

    ResolveModuleId = std::move(InResolve);
    LoadSourceByModuleId = std::move(InLoadSource);

    v8::Isolate::Scope isolate_scope(Isolate);
    v8::HandleScope handle_scope(Isolate);
    v8::Local<v8::Context> ctx = Context.Get(Isolate);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch try_catch(Isolate);

    v8::Local<v8::Module> root;
    if (!GetOrCompileModule(/*referrerResolvedId=*/"", EntrySpecifier, root))
        return v8::MaybeLocal<v8::Module>();

    UE_LOG(LogJs, Verbose, TEXT("LoadModule: compiled root, status=%d"), (int)root->GetStatus());

    if (root->GetStatus() < v8::Module::kInstantiated)
    {
        UE_LOG(LogJs, Verbose, TEXT("LoadModule: instantiating"));
        v8::Maybe<bool> ok = root->InstantiateModule(ctx, &FV8ModuleManager::ResolveModuleCallback);
        if (ok.IsNothing() || !ok.FromJust())
        {
            if (try_catch.HasCaught())
            {
                V8Util::LogTryCatch(Isolate, try_catch, TEXT("LoadModule/Instantiate"));
            }
            UE_LOG(LogJs, Error, TEXT("LoadModule: instantiate failed"));
            return v8::MaybeLocal<v8::Module>();
        }
    }

    if (root->GetStatus() < v8::Module::kEvaluated)
    {
        UE_LOG(LogJs, Verbose, TEXT("LoadModule: evaluating"));
        v8::Local<v8::Value> eval;
        if (!root->Evaluate(ctx).ToLocal(&eval))
        {
            if (try_catch.HasCaught())
            {
                V8Util::LogTryCatch(Isolate, try_catch, TEXT("LoadModule/Evaluate"));
            }
            UE_LOG(LogJs, Error, TEXT("LoadModule: evaluate failed"));
            return v8::MaybeLocal<v8::Module>();
        }
    }

    UE_LOG(LogJs, Verbose, TEXT("LoadModule complete. entry='%s'"), *FString(EntrySpecifier.data()));
    return root;
}

void FV8ModuleManager::UnloadAll()
{
    for (auto& kv : ModuleCache) kv.second.Reset();
    ModuleCache.clear();
    ModuleIdByPtr.clear();

    ResolveModuleId = nullptr;
    LoadSourceByModuleId = nullptr;

    if (!Context.IsEmpty()) 
    {
        Context.Reset();
    }
    Isolate = nullptr;
}

v8::MaybeLocal<v8::Module> FV8ModuleManager::ResolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> import_assertions,
    v8::Local<v8::Module> referrer)
{
    (void)import_assertions;
    v8::Isolate* isolate = context->GetIsolate();
    FV8ModuleManager* mgr = GetManager(context, isolate);
    if (!mgr)
    {
        isolate->ThrowException(v8::Exception::Error(
            v8::String::NewFromUtf8Literal(isolate, "ModuleManager not bound")));
        return v8::MaybeLocal<v8::Module>();
    }

    std::string request = ToUtf8(isolate, specifier);
    std::string referrerId = mgr->LookupResolvedId(referrer);

    v8::Local<v8::Module> out;
    if (!mgr->GetOrCompileModule(referrerId, request, out))
    {
        return v8::MaybeLocal<v8::Module>();
    }

    return out;
}

bool FV8ModuleManager::GetOrCompileModule(std::string_view ReferrerResolvedId,
    std::string_view RequestSpecifier,
    v8::Local<v8::Module>& OutModule)
{
    if (!ResolveModuleId || !LoadSourceByModuleId)
    {
        ThrowJsError("Resolve/LoadSource callbacks not set.");
        return false;
    }

    std::string resolvedId;
    std::string err;
    if (!ResolveModuleId(ReferrerResolvedId, RequestSpecifier, resolvedId, err))
    {
        ThrowJsError(err.empty() ? "ResolveModuleId failed." : err.c_str());
        return false;
    }

    UE_LOG(LogJs, Verbose, TEXT("GetOrCompileModule: resolved '%s' -> '%s'"), *FString(RequestSpecifier.data()), *FString(resolvedId.c_str()));

    if (auto it = ModuleCache.find(resolvedId); it != ModuleCache.end())
    {
        UE_LOG(LogJs, Verbose, TEXT("GetOrCompileModule: cache hit '%s'"), *FString(resolvedId.c_str()));
        OutModule = it->second.Get(Isolate);
        return true;
    }

    std::string sourceUtf8;
    if (!LoadSourceByModuleId(resolvedId, sourceUtf8, err))
    {
		UE_LOG(LogJs, Error, TEXT("GetOrCompileModule: LoadSourceByModuleId failed for '%s': %s"), *FString(resolvedId.c_str()), *FString(err.c_str()));
        ThrowJsError(err.empty() ? "LoadSourceByModuleId failed." : err.c_str());
        return false;
    }

    UE_LOG(LogJs, Verbose, TEXT("GetOrCompileModule: compiling '%s' (source length=%d)"), *FString(resolvedId.c_str()), (int)sourceUtf8.size());

    v8::Local<v8::Context> ctx = Context.Get(Isolate);
    v8::Context::Scope cs(ctx);

    v8::Local<v8::String> sourceStr;
    if (!v8::String::NewFromUtf8(Isolate, sourceUtf8.c_str(), v8::NewStringType::kNormal).ToLocal(&sourceStr))
    {
		UE_LOG(LogJs, Error, TEXT("GetOrCompileModule: Create source string failed for '%s'"), *FString(resolvedId.c_str()));
        ThrowJsError("Create source string failed.");
        return false;
    }

    v8::Local<v8::String> resourceName;
    if (!v8::String::NewFromUtf8(Isolate, resolvedId.c_str(),
        v8::NewStringType::kNormal).ToLocal(&resourceName))
    {
		UE_LOG(LogJs, Error, TEXT("GetOrCompileModule: Create resourceName failed for '%s'"), *FString(resolvedId.c_str()));
        ThrowJsError("Create resourceName failed.");
        return false;
    }

    v8::ScriptOrigin origin(resourceName, 0, 0, false, -1,
        v8::Local<v8::Value>(), false, false, true);

    v8::ScriptCompiler::Source sc(sourceStr, origin);

    v8::TryCatch try_catch(Isolate);

    v8::Local<v8::Module> mod;
    if (!v8::ScriptCompiler::CompileModule(Isolate, &sc).ToLocal(&mod)) {
		UE_LOG(LogJs, Error, TEXT("GetOrCompileModule: CompileModule failed for '%s'"), *FString(resolvedId.c_str()));
        if (try_catch.HasCaught())
        {
            V8Util::LogTryCatch(Isolate, try_catch, TEXT("GetOrCompileModule/Compile"));
        }
        return false;
    }
    ModuleCache.emplace(resolvedId, v8::Global<v8::Module>(Isolate, mod));
    RememberResolvedId(mod, resolvedId);

    OutModule = mod;
    return true;
}

void FV8ModuleManager::RememberResolvedId(v8::Local<v8::Module> Module, const std::string& ResolvedId)
{
    void* key = static_cast<void*>(*Module);
    ModuleIdByPtr[key] = ResolvedId;
}

std::string FV8ModuleManager::LookupResolvedId(v8::Local<v8::Module> Module) const
{
    void* key = static_cast<void*>(*Module);
    auto it = ModuleIdByPtr.find(key);
    return (it == ModuleIdByPtr.end()) ? std::string() : it->second;
}

FV8ModuleManager* FV8ModuleManager::GetManager(v8::Local<v8::Context> ctx, v8::Isolate* isolate)
{
    if (ctx->GetNumberOfEmbedderDataFields() > kEmbedderSlot)
    {
        void* p = ctx->GetAlignedPointerFromEmbedderData(kEmbedderSlot);
        if (p) return static_cast<FV8ModuleManager*>(p);
    }
    void* p = isolate->GetData(kIsolateSlot);
    return p ? static_cast<FV8ModuleManager*>(p) : nullptr;
}

std::string FV8ModuleManager::ToUtf8(v8::Isolate* isolate, v8::Local<v8::String> s)
{
    v8::String::Utf8Value v(isolate, s);
    return *v ? std::string(*v, v.length()) : std::string();
}

void FV8ModuleManager::ThrowJsError(const char* msg)
{
    Isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(Isolate, msg, v8::NewStringType::kNormal).ToLocalChecked()));
}

void FV8ModuleManager::ExcuteFunction(std::string_view ModuleId, 
    std::string_view FunctionName,
    std::span<v8::Local<v8::Value>> Args,
    v8::Local<v8::Value>& OutResult)
{
    UE_LOG(LogJs, Verbose, TEXT("ExcuteFunction: ModuleId='%s', FunctionName='%s'"), *FString(ModuleId.data()), *FString(FunctionName.data()));

    v8::Isolate* isolate = Isolate;
    if (!isolate || Context.IsEmpty())
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Isolate or Context not initialized."));
        return;
    }

    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> ctx = Context.Get(isolate);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch try_catch(isolate);

    auto it = ModuleCache.find(std::string(ModuleId));
    if (it == ModuleCache.end())
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Module '%s' not found in cache."), *FString(ModuleId.data()));
        return;
    }

    v8::Local<v8::Module> foundedModule = it->second.Get(isolate);
    if (foundedModule.IsEmpty())
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Found module is empty for '%s'."), *FString(ModuleId.data()));
        return;
    }

    UE_LOG(LogJs, Verbose, TEXT("ExcuteFunction: module status=%d"), (int)foundedModule->GetStatus());

    if (foundedModule->GetStatus() < v8::Module::kEvaluated)
    {
        UE_LOG(LogJs, Verbose, TEXT("ExcuteFunction: evaluating module"));
        v8::MaybeLocal<v8::Value> EvalResult = foundedModule->Evaluate(ctx);
        if (EvalResult.IsEmpty())
        {
            if (try_catch.HasCaught())
            {
                V8Util::LogTryCatch(isolate, try_catch, TEXT("ExcuteFunction/Evaluate"));
            }
            UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Evaluate failed for module '%s'."), *FString(ModuleId.data()));
            return;
        }
        isolate->PerformMicrotaskCheckpoint();
    }

    UE_LOG(LogJs, Verbose, TEXT("ExcuteFunction: retrieving module namespace"));
    v8::Local<v8::Value> moduleNsVal = foundedModule->GetModuleNamespace();
    if (moduleNsVal.IsEmpty() || !moduleNsVal->IsObject())
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Module namespace is not an object for '%s'."), *FString(ModuleId.data()));
        return;
    }
    v8::Local<v8::Object> moduleNameSpace = moduleNsVal.As<v8::Object>();

    v8::Local<v8::String> funKey;
    if (!v8::String::NewFromUtf8(isolate, std::string(FunctionName).c_str(), v8::NewStringType::kNormal).ToLocal(&funKey))
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Failed to create function key string for '%s'."), *FString(FunctionName.data()));
        return;
    }

    UE_LOG(LogJs, Verbose, TEXT("ExcuteFunction: looking up export"));
    v8::Local<v8::Value> funVal;
    if (!moduleNameSpace->Get(ctx, funKey).ToLocal(&funVal) || !funVal->IsFunction())
    {
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Export '%s' not found or not a function in module '%s'."), *FString(FunctionName.data()), *FString(ModuleId.data()));
        return;
    }
    v8::Local<v8::Function> targetFn = funVal.As<v8::Function>();

    v8::Local<v8::Value> ret;
    if (!targetFn->Call(ctx, v8::Undefined(isolate), Args.size(), Args.data()).ToLocal(&ret))
    {
        if (try_catch.HasCaught())
        {
            V8Util::LogTryCatch(isolate, try_catch, TEXT("ExcuteFunction/Call"));
        }
        UE_LOG(LogJs, Error, TEXT("ExcuteFunction: Call failed for '%s' in module '%s'."), *FString(FunctionName.data()), *FString(ModuleId.data()));
        return;
    }

    OutResult = ret;
}

} // namespace rinrin::jsruntime