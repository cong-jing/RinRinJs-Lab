// Copyright Epic Games, Inc. All Rights Reserved.

#include "V8Runtime.h"

#include "CoreMinimal.h"
#include "Inspector/V8Inspector.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Util/Converter.h"
#include "Util/Log.h"
#include "V8/V8Includes.h"
#include "Package.h"

namespace rinrin::uejs
{

    FV8Runtime::FV8Runtime() = default;
    FV8Runtime::~FV8Runtime() = default;

    static bool bProcessInitialized = false;

    void FV8Runtime::EnsureV8ProcessInitialized()
    {
        // V8 requires process-wide initialization only once.
        if (bProcessInitialized)
            return;

        v8::V8::InitializeICUDefaultLocation(nullptr);
        v8::V8::InitializeExternalStartupData(nullptr);

        V8Platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(V8Platform.get());
        v8::V8::Initialize();

        bProcessInitialized = true;
    }

    void FV8Runtime::FinalizeV8Process()
    {
        // 确保先清理执行上下文（以防 DestroyExecutionContext 没被调用）
        if (bIsInitialized)
        {
            DestroyExecutionContext();
        }

        v8::V8::Dispose();
        v8::V8::DisposePlatform();
        V8Platform.reset();
        bProcessInitialized = false;
    }

    void FV8Runtime::CreateExecutionContext()
    {
        if (bIsInitialized)
        {
            UEJS_LOG(LogJs, Warning, "V8 already initialized");
            return;
        }
        EnsureV8ProcessInitialized();

        Esm.reset();
        V8ContextGlobal.Reset();
        ArrayBufferAllocator.reset();
        V8Isolate.reset();

        UEJS_LOG(LogJs, Log, "Initializing V8 engine...");

        // Step 3: Create Isolate
        v8::Isolate::CreateParams create_params;
        ArrayBufferAllocator.reset(
            v8::ArrayBuffer::Allocator::NewDefaultAllocator());
        create_params.array_buffer_allocator = ArrayBufferAllocator.get();

        UEJS_LOG(LogJs, Log, "Initializing V8 engine...  5555");
        v8::Isolate *isolate = v8::Isolate::New(create_params);
        V8Isolate.reset(isolate);

        if (!V8Isolate)
        {
            UEJS_LOG(LogJs, Error, "Failed to create V8 Isolate");
            FMessageDialog::Open(
                EAppMsgType::Ok, NSLOCTEXT("FV8Runtime", "IsolateCreationError",
                                           "Failed to create V8 Isolate"));
            return;
        }
        isolate->Enter();

        // Step 4: Create a Global context handle
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);

        v8::Local<v8::Context> ctx = v8::Context::New(isolate);
        V8ContextGlobal.Reset(isolate, ctx);

        // FV8Console::InjectConsole(isolate, ctx);

        // 启动 Inspector Transport (WebSocket)
        Inspector = std::make_unique<rinrin::uejs::inspector::FV8Inspector>();
        Inspector->Start(V8Platform.get(), isolate, ctx);

        bIsInitialized = true;
        UEJS_LOG(LogJs, Log, "V8 engine initialized successfully");
    }

    void FV8Runtime::DestroyExecutionContext()
    {
        UEJS_LOG(LogJs, Log, "DestroyExecutionContext");
        // 停止 Inspector Host
        if (Inspector)
        {
            Inspector->Shutdown();
            Inspector.reset();
        }

        if (Esm)
        {
            Esm->UnloadAll();
            Esm.reset();
        }
        V8ContextGlobal.Reset();
        if (V8Isolate)
        {
            V8Isolate->Exit();
            V8Isolate.reset();
        }

        ArrayBufferAllocator.reset();
        bIsInitialized = false;
    }

    bool FV8Runtime::IsContextCreated() const
    {
        return bIsInitialized && (V8Isolate.get() != nullptr);
    }

    TExpected<void> FV8Runtime::LoadJsModule(const std::string_view ModuleName,
                                             FResolveModuleIdFn InResolve,
                                             FLoadSourceByModuleIdFn InLoadSource)
    {
        if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
        {
            // UEJS_LOG(LogJs, Error, TEXT("V8 is not initialized. Cannot load JS
            // module."));
            return UEJS_MAKE_ERROR("V8 not initialized");
        }
        v8::Isolate *isolate = V8Isolate.get();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);

        v8::Local<v8::Context> ctx = V8ContextGlobal.Get(isolate);
        v8::Context::Scope context_scope(ctx);

        if (!Esm)
        {
            Esm.reset(new FPackage(isolate, ctx));
        }

        UEJS_LOG(LogJs, Log, "Loading JS module: {}", ModuleName);
        UEJS_RETURN_IF_ERROR("Loading JS module", Esm->LoadModule(ModuleName, InResolve, InLoadSource));

        return TExpected<void>();
    }

    TExpected<FValueFromJs> FV8Runtime::EvaluateScript(std::string_view ScriptUtf8)
    {
        if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
        {
            return UEJS_MAKE_ERROR("V8 not initialized");
        }
        // Enter isolate scope
        v8::Isolate::Scope isolate_scope(V8Isolate.get());
        v8::HandleScope handle_scope(V8Isolate.get());

        // Get Global context
        v8::Local<v8::Context> context = V8ContextGlobal.Get(V8Isolate.get());
        v8::Context::Scope context_scope(context);

        // Create source string directly from UTF-8 view
        v8::Local<v8::String> source =
            util::MakeV8String(V8Isolate.get(), ScriptUtf8);

        // Compile script
        v8::TryCatch try_catch(V8Isolate.get());
        v8::MaybeLocal<v8::Script> maybe_script =
            v8::Script::Compile(context, source);

        v8::Local<v8::Script> script;
        if (!maybe_script.ToLocal(&script))
        {
            return UEJS_MAKE_ERROR_WITH_JS_STACK(
                FJsStackInfo(V8Isolate.get(), try_catch), "Compilation failed");
        }

        // Run script
        v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);

        v8::Local<v8::Value> result;
        if (!maybe_result.ToLocal(&result))
        {
            return UEJS_MAKE_ERROR_WITH_JS_STACK(
                FJsStackInfo(V8Isolate.get(), try_catch), "Evaluate script failed");
        }

        return util::MakeValueFromV8(V8Isolate.get(), context, result);
    }

    TExpected<FValueFromJs> FV8Runtime::ExecuteJsFunction(const std::string_view ModuleName,
                                                          const std::string_view ObjectName,
                                                          const std::string_view FunctionName,
                                                          const std::span<FValueIntoJs> &Args)
    {

        if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
        {
            return UEJS_MAKE_ERROR("V8 not initialized");
        }
        v8::Isolate *isolate = V8Isolate.get();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);

        v8::Local<v8::Context> ctx = V8ContextGlobal.Get(isolate);
        v8::Context::Scope context_scope(ctx);

        if (!Esm)
        {
            return UEJS_MAKE_ERROR("JS Module Manager not initialized");
        }

        return Esm->ExecuteJsFunction(
            ModuleName, ObjectName, FunctionName, Args);
    }

} // namespace rinrin::uejs
