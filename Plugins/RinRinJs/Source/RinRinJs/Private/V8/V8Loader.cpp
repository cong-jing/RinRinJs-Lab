// Copyright Epic Games, Inc. All Rights Reserved.

#include "V8Loader.h"
#include "V8ModuleManager.h"
#include "V8Console.h"
#include "Inspector/V8Inspector.h"
#include "Util/Log.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

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

	FV8Loader::FV8Loader() = default;
	FV8Loader::~FV8Loader() = default;

	static bool bProcessInitialized = false;

	void FV8Loader::EnsureV8ProcessInitialized()
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

	void FV8Loader::FinalizeV8Process()
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

	void FV8Loader::CreateExecutionContext()
	{
		if (bIsInitialized)
		{
			UEJS_LOG(LogJs, Warning, "V8 already initialized");
			return;
		}
		EnsureV8ProcessInitialized();

		JsModuleManager.reset();
		V8ContextGlobal.Reset();
		ArrayBufferAllocator.reset();
		V8Isolate.reset();

		UEJS_LOG(LogJs, Log, "Initializing V8 engine...");

		// Step 3: Create Isolate
		v8::Isolate::CreateParams create_params;
		ArrayBufferAllocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
		create_params.array_buffer_allocator = ArrayBufferAllocator.get();

		UEJS_LOG(LogJs, Log, "Initializing V8 engine...  5555");
		v8::Isolate *isolate = v8::Isolate::New(create_params);
		V8Isolate.reset(isolate);

		if (!V8Isolate)
		{
			UEJS_LOG(LogJs, Error, "Failed to create V8 Isolate");
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("FV8Loader", "IsolateCreationError", "Failed to create V8 Isolate"));
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

	void FV8Loader::DestroyExecutionContext()
	{
		UEJS_LOG(LogJs, Log, "DestroyExecutionContext");
		// 停止 Inspector Host
		if (Inspector)
		{
			Inspector->Shutdown();
			Inspector.reset();
		}

		if (JsModuleManager)
		{
			JsModuleManager->UnloadAll();
			JsModuleManager.reset();
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

	bool FV8Loader::IsContextCreated() const
	{
		return bIsInitialized && (V8Isolate.get() != nullptr);
	}

	void FV8Loader::EnsureModuleManager()
	{
		if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
			return;
		if (JsModuleManager)
			return;

		v8::Isolate *isolate = V8Isolate.get();
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		v8::Local<v8::Context> ctx = V8ContextGlobal.Get(isolate);
		v8::Context::Scope context_scope(ctx);

		JsModuleManager.reset(new FV8ModuleManager(isolate, ctx));
	}

	std::string FV8Loader::ExecuteJavaScript(std::string_view ScriptUtf8)
	{
		if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
		{
			UEJS_LOG(LogJs, Error, "V8 is not initialized. Cannot execute JavaScript.");
			return std::string("Error: V8 not initialized");
		}

		UEJS_LOG(LogJs, Verbose, "Executing JavaScript (len={})", (int)ScriptUtf8.size());

		// Enter isolate scope
		v8::Isolate::Scope isolate_scope(V8Isolate.get());
		v8::HandleScope handle_scope(V8Isolate.get());

		// Get Global context
		v8::Local<v8::Context> context = V8ContextGlobal.Get(V8Isolate.get());
		v8::Context::Scope context_scope(context);

		// Create source string directly from UTF-8 view
		v8::Local<v8::String> source;
		if (!v8::String::NewFromUtf8(V8Isolate.get(), ScriptUtf8.data(), v8::NewStringType::kNormal, (int)ScriptUtf8.size()).ToLocal(&source))
		{
			UEJS_LOG(LogJs, Error, "Failed to create V8 source string");
			return std::string("Error: Failed to create source string");
		}

		// Compile script
		v8::TryCatch try_catch(V8Isolate.get());
		v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(context, source);

		v8::Local<v8::Script> script;
		if (!maybe_script.ToLocal(&script))
		{
			// Get exception message
			v8::Local<v8::Value> exception = try_catch.Exception();
			v8::String::Utf8Value exception_str(V8Isolate.get(), exception);
			const char *exceptionCStr = *exception_str ? *exception_str : "";
			UEJS_LOG(LogJs, Error, "Failed to compile script: {}", exceptionCStr);
			return std::string("Error: Compilation failed - ") + exceptionCStr;
		}

		// Run script
		v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);

		v8::Local<v8::Value> result;
		if (!maybe_result.ToLocal(&result))
		{
			// Get exception message
			v8::Local<v8::Value> exception = try_catch.Exception();
			v8::String::Utf8Value exception_str(V8Isolate.get(), exception);
			const char *exceptionCStr = *exception_str ? *exception_str : "";
			UEJS_LOG(LogJs, Error, "Failed to execute script: {}", exceptionCStr);
			return std::string("Error: Execution failed - ") + exceptionCStr;
		}

		// v8::String::Utf8Value utf8(V8Isolate.get(), result);
		// if (*utf8)
		// {
		// 	UEJS_LOG(LogJs, Log, "JavaScript executed successfully. Result: {}", *utf8);
		// 	return std::string(*utf8);
		// }
		// else
		// {
		// 	UEJS_LOG(LogJs, Warning, "JavaScript executed but result is empty");
		// 	return std::string();
		// }
		auto result_str = result->ToString(context).ToLocalChecked();
		v8::String::Utf8Value utf8(V8Isolate.get(), result_str);
		UEJS_LOG(LogJs, Log, "JavaScript executed successfully. {}", *utf8);

		return std::string(*utf8);
	}

	TExpected<void> FV8Loader::LoadJsModule(const std::string_view ModuleName, FResolveModuleIdFn InResolve, FLoadSourceByModuleIdFn InLoadSource)
	{
		if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
		{
			// UEJS_LOG(LogJs, Error, TEXT("V8 is not initialized. Cannot load JS module."));
			return UEJS_MAKE_ERROR("V8 not initialized");
		}
		v8::Isolate *isolate = V8Isolate.get();
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		v8::Local<v8::Context> ctx = V8ContextGlobal.Get(isolate);
		v8::Context::Scope context_scope(ctx);

		if (!JsModuleManager)
		{
			JsModuleManager.reset(new FV8ModuleManager(isolate, ctx));
		}
		UEJS_LOG(LogJs, Log, "Loading JS module: {}", ModuleName);
		UEJS_RETURN_IF_ERROR("Loading JS module", JsModuleManager->LoadModule(ModuleName, InResolve, InLoadSource));

		return TExpected<void>();
	}

} // namespace rinrin::uejs
