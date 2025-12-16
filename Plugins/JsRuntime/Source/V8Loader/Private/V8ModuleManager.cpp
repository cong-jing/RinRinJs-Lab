#include "V8ModuleManager.h"
#include "V8Logger.h"

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

void FV8ModuleManager::Setup(v8::Isolate* InIsolate,
    v8::Local<v8::Context> InContext,
    FResolveModuleIdFn InResolve,
    FLoadSourceByModuleIdFn InLoadSource)
{
    UnloadAll();
    Isolate = InIsolate;

    v8::Isolate::Scope isolate_scope(Isolate);
    v8::HandleScope handle_scope(Isolate);
    Context.Reset(Isolate, InContext);

    ResolveModuleId = std::move(InResolve);
    LoadSourceByModuleId = std::move(InLoadSource);

    // 让 resolver 静态函数能找回 this
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


v8::MaybeLocal<v8::Module> FV8ModuleManager::LoadModule(std::string_view EntrySpecifier)
{
    if (!Isolate || Context.IsEmpty())
        return v8::MaybeLocal<v8::Module>();

    v8::Isolate::Scope isolate_scope(Isolate);
    v8::HandleScope handle_scope(Isolate);
    v8::Local<v8::Context> ctx = Context.Get(Isolate);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch try_catch(Isolate);

    v8::Local<v8::Module> root;
    if (!GetOrCompileModule(/*referrerResolvedId=*/"", EntrySpecifier, root))
        return v8::MaybeLocal<v8::Module>();

    if (root->GetStatus() < v8::Module::kInstantiated)
    {
        v8::Maybe<bool> ok = root->InstantiateModule(ctx, &FV8ModuleManager::ResolveModuleCallback);
        if (ok.IsNothing() || !ok.FromJust())
            return v8::MaybeLocal<v8::Module>();
        if (ok.IsNothing() || !ok.FromJust()) {
            if (try_catch.HasCaught()) {
				V8LogHelper::LogTryCatch(Isolate, try_catch, TEXT("LoadModule"));
            }
            return v8::MaybeLocal<v8::Module>();
        }
    }

    if (root->GetStatus() < v8::Module::kEvaluated)
    {
        v8::Local<v8::Value> eval;
        if (!root->Evaluate(ctx).ToLocal(&eval))
            return v8::MaybeLocal<v8::Module>();
    }

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

    // TODO: ue:core 之类内建模块可在这里直接分支处理
    // if (request == "ue:core") return mgr->GetOrCreateUeCore(...);

    v8::Local<v8::Module> out;
    if (!mgr->GetOrCompileModule(referrerId, request, out))
        return v8::MaybeLocal<v8::Module>();

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

    // 1) 先 Resolve（无 IO）
    std::string resolvedId;
    std::string err;
    if (!ResolveModuleId(ReferrerResolvedId, RequestSpecifier, resolvedId, err))
    {
        ThrowJsError(err.empty() ? "ResolveModuleId failed." : err.c_str());
        return false;
    }

    // 2) cache 命中：无 IO 直接返回
    if (auto it = ModuleCache.find(resolvedId); it != ModuleCache.end())
    {
        OutModule = it->second.Get(Isolate);
        return true;
    }

    // 3) miss：再 LoadSource（有 IO）
    std::string sourceUtf8;
    if (!LoadSourceByModuleId(resolvedId, sourceUtf8, err))
    {
        ThrowJsError(err.empty() ? "LoadSourceByModuleId failed." : err.c_str());
        return false;
    }

    // 4) Compile
    v8::Local<v8::Context> ctx = Context.Get(Isolate);
    v8::Context::Scope cs(ctx);

    v8::Local<v8::String> sourceStr;
    if (!v8::String::NewFromUtf8(Isolate, sourceUtf8.c_str(),
        v8::NewStringType::kNormal).ToLocal(&sourceStr))
    {
        ThrowJsError("Create source string failed.");
        return false;
    }

    v8::Local<v8::String> resourceName;
    if (!v8::String::NewFromUtf8(Isolate, resolvedId.c_str(),
        v8::NewStringType::kNormal).ToLocal(&resourceName))
    {
        ThrowJsError("Create resourceName failed.");
        return false;
    }

    v8::ScriptOrigin origin(resourceName, 0, 0, false, -1,
        v8::Local<v8::Value>(), false, false, true);

    v8::ScriptCompiler::Source sc(sourceStr, origin);

    v8::Local<v8::Module> mod;
    if (!v8::ScriptCompiler::CompileModule(Isolate, &sc).ToLocal(&mod))
        return false;

    // 5) 立刻 cache（循环依赖需要）
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