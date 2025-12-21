#include "V8/V8ModuleManager.h"
#include "Common/LogMacros.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace rinrin::uejs
{
    FV8ModuleManager::FV8ModuleManager(
        v8::Isolate *InIsolate,
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

    TExpected<v8::Local<v8::Module>> FV8ModuleManager::LoadModule(
        std::string_view EntrySpecifier,
        FResolveModuleIdFn InResolve,
        FLoadSourceByModuleIdFn InLoadSource)
    {
        UEJS_LOG(LogJs, Verbose, "entry='%s'", *FString(EntrySpecifier.data()));

        if (!Isolate || Context.IsEmpty())
            return Err(FError("invalid isolate or context", UEJS_HERE));

        ResolveModuleId = std::move(InResolve);
        LoadSourceByModuleId = std::move(InLoadSource);

        v8::Isolate::Scope isolate_scope(Isolate);
        v8::HandleScope handle_scope(Isolate);
        v8::Local<v8::Context> ctx = Context.Get(Isolate);
        v8::Context::Scope context_scope(ctx);

        v8::TryCatch try_catch(Isolate);

        TExpected<v8::Local<v8::Module>> rootExpected = GetOrCompileModule(/*referrerResolvedId=*/"", EntrySpecifier);
        if (!rootExpected)
        {
            return Err(rootExpected.Error());
        }
        v8::Local<v8::Module> root = rootExpected.Value();

        UEJS_LOG(LogJs, Verbose, "compiled root, status=%d", (int)root->GetStatus());

        if (root->GetStatus() < v8::Module::kInstantiated)
        {
            UEJS_LOG(LogJs, Verbose, "instantiating");
            v8::Maybe<bool> ok = root->InstantiateModule(ctx, &FV8ModuleManager::ResolveModuleCallback);
            if (ok.IsNothing() || !ok.FromJust())
            {
                FError err("instantiate failed", UEJS_HERE);
                err.SetV8(FV8TryCatchInfo(Isolate, try_catch));
                err.Log(LogJs, ELogVerbosity::Error);
                return Err(MoveTemp(err));
            }
        }

        if (root->GetStatus() < v8::Module::kEvaluated)
        {
            UEJS_LOG(LogJs, Verbose, "evaluating");
            v8::Local<v8::Value> eval;
            if (!root->Evaluate(ctx).ToLocal(&eval))
            {
                // UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, false, TEXT("LoadModule: evaluate failed"));
                FError err("LoadModule: evaluate failed", UEJS_HERE);
                err.SetV8(FV8TryCatchInfo(Isolate, try_catch));
                err.Log(LogJs, ELogVerbosity::Error);
                return Err(MoveTemp(err));
            }
        }

        UEJS_LOG(LogJs, Verbose, "LoadModule complete. entry='%s'", *FString(EntrySpecifier.data()));
        return root;
    }

    void FV8ModuleManager::UnloadAll()
    {
        for (auto &kv : ModuleCache)
            kv.second.Reset();
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
        v8::Isolate *isolate = context->GetIsolate();
        FV8ModuleManager *mgr = GetManager(context, isolate);
        if (!mgr)
        {
            isolate->ThrowException(v8::Exception::Error(
                v8::String::NewFromUtf8Literal(isolate, "ModuleManager not bound")));
            return v8::MaybeLocal<v8::Module>();
        }

        std::string request = ToUtf8(isolate, specifier);
        std::string referrerId = mgr->LookupResolvedId(referrer);

        TExpected<v8::Local<v8::Module>> result = mgr->GetOrCompileModule(referrerId, request);
        if (!result)
        {
            return v8::MaybeLocal<v8::Module>();
        }

        return result.Value();
    }

    TExpected<v8::Local<v8::Module>> FV8ModuleManager::GetOrCompileModule(std::string_view ReferrerResolvedId,
                                                                          std::string_view RequestSpecifier)
    {
        if (!ResolveModuleId || !LoadSourceByModuleId)
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "Resolve/LoadSource callbacks not set.");
        }

        std::string resolvedId;
        std::string err;

        if (!ResolveModuleId(ReferrerResolvedId, RequestSpecifier, resolvedId, err))
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(
                LogJs,
                "GetOrCompileModule: ResolveModuleId failed for '%s': %s",
                *FString(RequestSpecifier.data()),
                *FString(err.c_str()));
        }

        UEJS_LOG(LogJs, Verbose, "GetOrCompileModule: resolved '%s' -> '%s'", *FString(RequestSpecifier.data()), *FString(resolvedId.c_str()));

        if (auto it = ModuleCache.find(resolvedId); it != ModuleCache.end())
        {
            UEJS_LOG(LogJs, Verbose, "GetOrCompileModule: cache hit '%s'", *FString(resolvedId.c_str()));
            return it->second.Get(Isolate);
        }

        std::string sourceUtf8;
        if (!LoadSourceByModuleId(resolvedId, sourceUtf8, err))
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(
                LogJs,
                "LoadSourceByModuleId failed for '%s': %s",
                *FString(resolvedId.c_str()),
                *FString(err.c_str()));
        }

        UEJS_LOG(LogJs, Verbose,
                 "compiling '%s' (source length=%d)",
                 *FString(resolvedId.c_str()), (int)sourceUtf8.size());

        v8::Local<v8::Context> ctx = Context.Get(Isolate);
        v8::Context::Scope cs(ctx);

        v8::Local<v8::String> sourceStr;
        if (!v8::String::NewFromUtf8(Isolate, sourceUtf8.c_str(), v8::NewStringType::kNormal).ToLocal(&sourceStr))
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(
                LogJs,
                "Create source string failed for '%s'",
                *FString(resolvedId.c_str()));
        }

        v8::Local<v8::String> resourceName;
        if (!v8::String::NewFromUtf8(Isolate, resolvedId.c_str(),
                                     v8::NewStringType::kNormal)
                 .ToLocal(&resourceName))
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(
                LogJs,
                "Create resourceName failed for '%s'",
                *FString(resolvedId.c_str()));
        }

        v8::ScriptOrigin origin(resourceName, 0, 0, false, -1,
                                v8::Local<v8::Value>(), false, false, true);

        v8::ScriptCompiler::Source sc(sourceStr, origin);

        v8::TryCatch try_catch(Isolate);

        v8::Local<v8::Module> mod;
        if (!v8::ScriptCompiler::CompileModule(Isolate, &sc).ToLocal(&mod))
        {
            FError compileErr("CompileModule failed", UEJS_HERE);
            compileErr.SetV8(FV8TryCatchInfo(Isolate, try_catch));
            compileErr.Log(LogJs, ELogVerbosity::Error);
            return Err(compileErr);
        }
        ModuleCache.emplace(resolvedId, v8::Global<v8::Module>(Isolate, mod));
        RememberResolvedId(mod, resolvedId);

        return mod;
    }

    void FV8ModuleManager::RememberResolvedId(v8::Local<v8::Module> Module, const std::string &ResolvedId)
    {
        void *key = static_cast<void *>(*Module);
        ModuleIdByPtr[key] = ResolvedId;
    }

    std::string FV8ModuleManager::LookupResolvedId(v8::Local<v8::Module> Module) const
    {
        void *key = static_cast<void *>(*Module);
        auto it = ModuleIdByPtr.find(key);
        return (it == ModuleIdByPtr.end()) ? std::string() : it->second;
    }

    FV8ModuleManager *FV8ModuleManager::GetManager(v8::Local<v8::Context> ctx, v8::Isolate *isolate)
    {
        if (ctx->GetNumberOfEmbedderDataFields() > kEmbedderSlot)
        {
            void *p = ctx->GetAlignedPointerFromEmbedderData(kEmbedderSlot);
            if (p)
                return static_cast<FV8ModuleManager *>(p);
        }
        void *p = isolate->GetData(kIsolateSlot);
        return p ? static_cast<FV8ModuleManager *>(p) : nullptr;
    }

    std::string FV8ModuleManager::ToUtf8(v8::Isolate *isolate, v8::Local<v8::String> s)
    {
        v8::String::Utf8Value v(isolate, s);
        return *v ? std::string(*v, v.length()) : std::string();
    }

    TExpected<void> FV8ModuleManager::ExcuteFunction(std::string_view ModuleId,
                                                     std::string_view FunctionName,
                                                     std::span<v8::Local<v8::Value>> Args,
                                                     v8::Local<v8::Value> &OutResult)
    {
        UEJS_LOG(LogJs, Verbose,
                 "ExcuteFunction: ModuleId='%s', FunctionName='%s'",
                 *FString(ModuleId.data()),
                 *FString(FunctionName.data()));

        v8::Isolate *isolate = Isolate;
        if (!isolate || Context.IsEmpty())
        {
            return Err(FError("ExcuteFunction: Isolate or Context not initialized.", UEJS_HERE));
        }

        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> ctx = Context.Get(isolate);
        v8::Context::Scope context_scope(ctx);

        v8::TryCatch try_catch(isolate);

        auto it = ModuleCache.find(std::string(ModuleId));
        if (it == ModuleCache.end())
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "ExcuteFunction: Module '%s' not found in cache.", *FString(ModuleId.data()));
        }

        v8::Local<v8::Module> foundedModule = it->second.Get(isolate);
        if (foundedModule.IsEmpty())
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "ExcuteFunction: Found module is empty for '%s'.", *FString(ModuleId.data()));
        }

        UEJS_LOG(LogJs, Verbose, "ExcuteFunction: module status=%d", (int)foundedModule->GetStatus());

        if (foundedModule->GetStatus() < v8::Module::kEvaluated)
        {
            UEJS_LOG(LogJs, Verbose, "ExcuteFunction: evaluating module");
            v8::MaybeLocal<v8::Value> EvalResult = foundedModule->Evaluate(ctx);
            if (EvalResult.IsEmpty())
            {
                FError err("ExcuteFunction: Evaluate failed", UEJS_HERE);
                err.SetV8(FV8TryCatchInfo(isolate, try_catch));
                err.Log(LogJs, ELogVerbosity::Error);
                return Err(MoveTemp(err));
            }
            isolate->PerformMicrotaskCheckpoint();
        }

        UEJS_LOG(LogJs, Verbose, "ExcuteFunction: retrieving module namespace");
        v8::Local<v8::Value> moduleNsVal = foundedModule->GetModuleNamespace();
        if (moduleNsVal.IsEmpty() || !moduleNsVal->IsObject())
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "ExcuteFunction: Module namespace is not an object for '%s'.", *FString(ModuleId.data()));
        }
        v8::Local<v8::Object> moduleNameSpace = moduleNsVal.As<v8::Object>();

        v8::Local<v8::String> funKey;
        if (!v8::String::NewFromUtf8(isolate, std::string(FunctionName).c_str(), v8::NewStringType::kNormal).ToLocal(&funKey))
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "ExcuteFunction: Failed to create function key string for '%s'.", *FString(FunctionName.data()));
        }

        UEJS_LOG(LogJs, Verbose, "ExcuteFunction: looking up export");
        v8::Local<v8::Value> funVal;
        if (!moduleNameSpace->Get(ctx, funKey).ToLocal(&funVal) || !funVal->IsFunction())
        {
            UEJS_LOG_ONCE_AND_RETURN_ERR(LogJs, "ExcuteFunction: Export '%s' not found or not a function in module '%s'.", *FString(FunctionName.data()), *FString(ModuleId.data()));
        }
        v8::Local<v8::Function> targetFn = funVal.As<v8::Function>();

        v8::Local<v8::Value> ret;
        if (!targetFn->Call(ctx, v8::Undefined(isolate), Args.size(), Args.data()).ToLocal(&ret))
        {
            FError err("ExcuteFunction: Call failed", UEJS_HERE);
            err.SetV8(FV8TryCatchInfo(isolate, try_catch));
            err.Log(LogJs, ELogVerbosity::Error);
            return Err(MoveTemp(err));
        }

        OutResult = ret;
        return TExpected<void>();
    }

} // namespace rinrin::uejs
