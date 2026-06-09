#include "Package.h"
#include "Util/Converter.h"
#include "Util/Log.h"

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
    FPackage::FPackage(
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

    FPackage::FPackage(
        v8::Isolate *InIsolate,
        v8::Local<v8::Context> InContext,
        FPackageInfo InInfo)
        : FPackage(InIsolate, InContext)
    {
        Info = std::move(InInfo);
    }

    TExpected<v8::Local<v8::Module>> FPackage::LoadModule(
        std::string_view EntrySpecifier,
        FResolveModuleIdFn InResolve,
        FLoadSourceByModuleIdFn InLoadSource)
    {
        UEJS_LOG(LogJs, Verbose, "LoadModule: entry='{}'", EntrySpecifier);

        if (!Isolate || Context.IsEmpty())
            return Err(FError("Invalid isolate or context", UEJS_HERE));

        ResolveModuleId = std::move(InResolve);
        LoadSourceByModuleId = std::move(InLoadSource);

        v8::Isolate::Scope isolate_scope(Isolate);
        v8::HandleScope handle_scope(Isolate);
        v8::Local<v8::Context> ctx = Context.Get(Isolate);
        v8::Context::Scope context_scope(ctx);

        v8::TryCatch try_catch(Isolate);

        TExpected<v8::Local<v8::Module>> compileResult = GetOrCompileModule(/*referrerResolvedId=*/"", EntrySpecifier);
        if (compileResult.HasError())
            return Err(compileResult.Error());

        v8::Local<v8::Module> root = compileResult.Value();

        UEJS_LOG(LogJs, Verbose, "LoadModule: compiled root, status={}", (int)root->GetStatus());

        if (root->GetStatus() < v8::Module::kInstantiated)
        {
            UEJS_LOG(LogJs, Verbose, "LoadModule: instantiating");
            v8::Maybe<bool> ok = root->InstantiateModule(ctx, &FPackage::ResolveModuleCallback);
            if (ok.IsNothing() || !ok.FromJust())
            {
                return UEJS_MAKE_ERROR_WITH_JS_STACK(
                    FJsStackInfo(Isolate, try_catch),
                    "InstantiateModule failed for '{}'",
                    EntrySpecifier);
            }
        }

        if (root->GetStatus() < v8::Module::kEvaluated)
        {
            UEJS_LOG(LogJs, Verbose, "LoadModule: evaluating");
            v8::Local<v8::Value> eval;
            if (!root->Evaluate(ctx).ToLocal(&eval))
            {
                return UEJS_MAKE_ERROR_WITH_JS_STACK(
                    FJsStackInfo(Isolate, try_catch), "Evaluate failed for '{}'", EntrySpecifier);
            }
        }

        UEJS_LOG(LogJs, Verbose, "LoadModule complete. entry='{}'", EntrySpecifier);
        return root;
    }

    void FPackage::UnloadAll()
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

    v8::MaybeLocal<v8::Module> FPackage::ResolveModuleCallback(
        v8::Local<v8::Context> context,
        v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_assertions,
        v8::Local<v8::Module> referrer)
    {
        (void)import_assertions;
        v8::Isolate *isolate = context->GetIsolate();
        FPackage *mgr = GetManager(context, isolate);
        if (!mgr)
        {
            isolate->ThrowException(v8::Exception::Error(
                v8::String::NewFromUtf8Literal(isolate, "ModuleManager not bound")));
            return v8::MaybeLocal<v8::Module>();
        }

        std::string request = util::ToStdString(isolate, specifier);
        std::string referrerId = mgr->LookupResolvedId(referrer);

        TExpected<v8::Local<v8::Module>> result = mgr->GetOrCompileModule(referrerId, request);
        if (!result)
        {
            result.Error().Log(LogJs, ELogVerbosity::Error);
            // 2) 关键：抛出异常给 V8（否则 V8 只能“空返回”，容易 fatal）
            const char *msg = result.Error().GetMessage().c_str();
            v8::Local<v8::String> jsMsg;
            if (v8::String::NewFromUtf8(isolate, msg, v8::NewStringType::kNormal).ToLocal(&jsMsg))
            {
                isolate->ThrowException(v8::Exception::Error(jsMsg));
            }
            else
            {
                isolate->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8Literal(isolate, "Failed to resolve/compile module")));
            }
            return {};
        }

        return result.Value();
    }

    TExpected<v8::Local<v8::Module>> FPackage::GetOrCompileModule(std::string_view ReferrerResolvedId,
                                                                  std::string_view RequestSpecifier)
    {
        if (!ResolveModuleId || !LoadSourceByModuleId)
        {
            return UEJS_MAKE_ERROR("Resolve/LoadSource callbacks not set.");
            // return UEJS_MAKE_ERROR(TEXT("Resolve/LoadSource callbacks not set."));
        }

        std::string resolvedId;
        std::string err;

        if (!ResolveModuleId(ReferrerResolvedId, RequestSpecifier, resolvedId, err))
        {
            return UEJS_MAKE_ERROR("GetOrCompileModule: ResolveModuleId failed for '{}': {}",
                                   RequestSpecifier,
                                   err);
        }

        UEJS_LOG(LogJs, Verbose, "GetOrCompileModule: resolved '{}' -> '{}'", RequestSpecifier, resolvedId);

        if (auto it = ModuleCache.find(resolvedId); it != ModuleCache.end())
        {
            UEJS_LOG(LogJs, Verbose, "GetOrCompileModule: cache hit '{}'", resolvedId);
            return it->second.Get(Isolate);
        }

        std::string sourceUtf8;
        if (!LoadSourceByModuleId(resolvedId, sourceUtf8, err))
        {
            return UEJS_MAKE_ERROR(
                "LoadSourceByModuleId failed for '{}': {}",
                resolvedId,
                err);
        }

        UEJS_LOG(LogJs, Log, "compiling '{}' {}", resolvedId, sourceUtf8);

        v8::Local<v8::Context> ctx = Context.Get(Isolate);
        v8::Context::Scope cs(ctx);

        v8::Local<v8::String> sourceStr;
        if (!v8::String::NewFromUtf8(Isolate, sourceUtf8.c_str(), v8::NewStringType::kNormal).ToLocal(&sourceStr))
        {
            return UEJS_MAKE_ERROR(
                "Create source string failed for '{}'",
                resolvedId);
        }

        v8::Local<v8::String> resourceName;
        if (!v8::String::NewFromUtf8(Isolate, resolvedId.c_str(),
                                     v8::NewStringType::kNormal)
                 .ToLocal(&resourceName))
        {
            return UEJS_MAKE_ERROR(
                "Create resourceName failed for '{}'",
                resolvedId);
        }

        v8::ScriptOrigin origin(resourceName, 0, 0, false, -1,
                                v8::Local<v8::Value>(), false, false, true);

        v8::ScriptCompiler::Source sc(sourceStr, origin);

        v8::TryCatch try_catch(Isolate);

        v8::Local<v8::Module> mod;
        if (!v8::ScriptCompiler::CompileModule(Isolate, &sc).ToLocal(&mod))
        {
            return UEJS_MAKE_ERROR(
                "CompileModule failed for '{}'",
                resolvedId);
        }
        ModuleCache.emplace(resolvedId, v8::Global<v8::Module>(Isolate, mod));
        RememberResolvedId(mod, resolvedId);

        return mod;
    }

    void FPackage::RememberResolvedId(v8::Local<v8::Module> Module, const std::string &ResolvedId)
    {
        void *key = static_cast<void *>(*Module);
        ModuleIdByPtr[key] = ResolvedId;
    }

    std::string FPackage::LookupResolvedId(v8::Local<v8::Module> Module) const
    {
        void *key = static_cast<void *>(*Module);
        auto it = ModuleIdByPtr.find(key);
        return (it == ModuleIdByPtr.end()) ? std::string() : it->second;
    }

    FPackage *FPackage::GetManager(v8::Local<v8::Context> ctx, v8::Isolate *isolate)
    {
        if (ctx->GetNumberOfEmbedderDataFields() > kEmbedderSlot)
        {
            void *p = ctx->GetAlignedPointerFromEmbedderData(kEmbedderSlot);
            if (p)
                return static_cast<FPackage *>(p);
        }
        void *p = isolate->GetData(kIsolateSlot);
        return p ? static_cast<FPackage *>(p) : nullptr;
    }

    TExpected<FValueFromJs> FPackage::ExecuteJsFunction(std::string_view ModuleId,
                                                        std::string_view ObjectName,
                                                        std::string_view FunctionName,
                                                        std::span<FValueIntoJs> Args)
    {
        UEJS_LOG(LogJs, Verbose,
                 "ExcuteFunction: ModuleId='{}', ObjectName='{}', FunctionName='{}'",
                 ModuleId,
                 ObjectName,
                 FunctionName);

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
            return UEJS_MAKE_ERROR("ExcuteFunction: Module '{}' not found in cache.", ModuleId);
        }

        v8::Local<v8::Module> foundedModule = it->second.Get(isolate);

        auto status = foundedModule->GetStatus();
        UEJS_LOG(LogJs, Verbose, "ExcuteFunction: module status={}", (int)status);

        if (status == v8::Module::kErrored)
        {
            // V8 已经把异常挂在 module 上了
            // v8::Local<v8::Value> ex = foundedModule->GetException();
            // TODO 这里可以把 ex 转成字符串/堆栈（建议你做一个 ReportV8ValueAsError）

            return UEJS_MAKE_ERROR_WITH_JS_STACK(
                FJsStackInfo(isolate, try_catch),
                "ExcuteFunction: Module '{}' is in errored state.", ModuleId);
        }

        // 如果缓存的是“已 compile 但未 instantiate”的 module，这里必须先 Instantiate
        if (status == v8::Module::kUninstantiated)
        {
            v8::Maybe<bool> ok = foundedModule->InstantiateModule(ctx, &FPackage::ResolveModuleCallback);
            if (ok.IsNothing() || !ok.FromJust())
            {
                return UEJS_MAKE_ERROR_WITH_JS_STACK(
                    FJsStackInfo(isolate, try_catch),
                    "ExcuteFunction: InstantiateModule failed for '{}'",
                    ModuleId);
            }
            status = foundedModule->GetStatus();
            if (status == v8::Module::kErrored)
            {
                // v8::Local<v8::Value> ex = foundedModule->GetException();

                return UEJS_MAKE_ERROR_WITH_JS_STACK(
                    FJsStackInfo(isolate, try_catch),
                    "ExcuteFunction: Module '{}' is in errored state after InstantiateModule.",
                    ModuleId);
            }
        }

        UEJS_LOG(LogJs, Verbose, "ExcuteFunction: retrieving module namespace");
        if (status != v8::Module::kInstantiated)
        {
            return UEJS_MAKE_ERROR(
                "ExcuteFunction: Module '{}' is in unexpected status {}.",
                ModuleId, (int)status);
        }
        v8::Local<v8::Value> moduleNsVal = foundedModule->GetModuleNamespace();
        if (moduleNsVal.IsEmpty() || !moduleNsVal->IsObject())
        {
            return UEJS_MAKE_ERROR("ExcuteFunction: Module namespace is not an object for '{}'.", ModuleId);
        }
        v8::Local<v8::Object> moduleNameSpace = moduleNsVal.As<v8::Object>();

        // TODO: split ObjectName by '.' and set it into array or vetor, then iterate to get the nested object
        if (!ObjectName.empty())
        {
        }

        v8::Local<v8::String> funKey;
        if (!v8::String::NewFromUtf8(isolate, std::string(FunctionName).c_str(), v8::NewStringType::kNormal).ToLocal(&funKey))
        {
            return UEJS_MAKE_ERROR(
                "ExcuteFunction: Failed to create function key string for '{}'.",
                FunctionName);
        }

        UEJS_LOG(LogJs, VeryVerbose, "ExcuteFunction: looking up export");
        v8::Local<v8::Value> funVal;
        if (!moduleNameSpace->Get(ctx, funKey).ToLocal(&funVal) || !funVal->IsFunction())
        {
            return UEJS_MAKE_ERROR(
                "ExcuteFunction: Export '{}' not found or not a function in module '{}'.",
                FunctionName, ModuleId);
        }
        v8::Local<v8::Function> targetFn = funVal.As<v8::Function>();

        std::vector<v8::Local<v8::Value>> v8Args;
        for (const FValueIntoJs &arg : Args)
        {
            TExpected<v8::Local<v8::Value>> v8Arg = rinrin::uejs::util::ToV8LocalValue(isolate, ctx, arg);
            if (v8Arg.HasError())
            {
                return Err(v8Arg.Error());
            }
            v8Args.push_back(v8Arg.Value());
        }
        v8::Local<v8::Value> ret;
        if (!targetFn->Call(ctx, v8::Undefined(isolate), Args.size(), v8Args.data()).ToLocal(&ret))
        {
            FError err("ExcuteFunction: Call failed", UEJS_HERE);
            err.SetJsStack(FJsStackInfo(isolate, try_catch));
            err.Log(LogJs, ELogVerbosity::Error);
            return Err(MoveTemp(err));
        }

        return util::MakeValueFromV8(isolate, Context.Get(isolate), ret);
    }

} // namespace rinrin::uejs
