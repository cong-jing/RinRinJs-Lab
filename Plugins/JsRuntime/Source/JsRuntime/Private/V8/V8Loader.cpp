// Copyright Epic Games, Inc. All Rights Reserved.


#include "V8/V8Loader.h"
#include "V8/V8Util.h"
#include "V8/V8ModuleManager.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4668)
#endif
#include "v8.h"
#include "libplatform/libplatform.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#define LOCTEXT_NAMESPACE "FV8Loader"

FV8Loader::FV8Loader() = default;
FV8Loader::~FV8Loader() = default;

void FV8Loader::InitializeV8()
{
	if (bIsInitialized)
	{
		UE_LOG(LogJs, Warning, TEXT("V8 already initialized"));
		return;
	}

	V8Platform.reset();
	ArrayBufferAllocator.reset();
	V8Isolate.reset();
	V8ContextGlobal.Reset();
	bIsInitialized = false;

	UE_LOG(LogJs, Log, TEXT("Initializing V8 engine..."));

	// Step 1: Initialize ICU and external startup data
	v8::V8::InitializeICUDefaultLocation(nullptr);
	v8::V8::InitializeExternalStartupData(nullptr);

	// Step 2: Create platform
	V8Platform = v8::platform::NewSingleThreadedDefaultPlatform();

	v8::V8::InitializePlatform(V8Platform.get());
	v8::V8::Initialize();

	// Step 3: Create Isolate
	v8::Isolate::CreateParams create_params;
	ArrayBufferAllocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
	create_params.array_buffer_allocator = ArrayBufferAllocator.get();

	V8Isolate.reset(v8::Isolate::New(create_params));

	if (!V8Isolate)
	{
		UE_LOG(LogJs, Error, TEXT("Failed to create V8 Isolate"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("IsolateCreationError", "Failed to create V8 Isolate"));
		return;
	}

	// Step 4: Create a Global context handle
	{
		v8::Isolate::Scope isolate_scope(V8Isolate.get());
		v8::HandleScope handle_scope(V8Isolate.get());

		v8::Local<v8::Context> ctx = v8::Context::New(V8Isolate.get());
		V8ContextGlobal.Reset(V8Isolate.get(), ctx);
	}

	bIsInitialized = true;
	UE_LOG(LogJs, Log, TEXT("V8 engine initialized successfully"));
}

void FV8Loader::ShutdownV8()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogJs, Log, TEXT("Shutting down V8 engine..."));

	if (JsModuleManager)
	{
		JsModuleManager->UnloadAll();
		JsModuleManager.reset();
	}


	// Dispose context
	if (!V8ContextGlobal.IsEmpty())
	{
		V8ContextGlobal.Reset();
	}

	// Dispose isolate
	if (V8Isolate)
	{
		V8Isolate.reset();
		UE_LOG(LogJs, Log, TEXT("V8 Isolate disposed"));
	}

	// Delete array buffer allocator
	ArrayBufferAllocator.reset();

	// Shutdown V8
	v8::V8::Dispose();

	// Dispose platform
	V8Platform.reset();

	bIsInitialized = false;
	UE_LOG(LogJs, Log, TEXT("V8 engine shut down successfully"));
}

bool FV8Loader::IsV8Loaded() const
{
	return bIsInitialized && (V8Isolate.get() != nullptr);
}

FString FV8Loader::ExecuteJavaScript(const FString& Script)
{
	if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
	{
		UE_LOG(LogJs, Error, TEXT("V8 is not initialized. Cannot execute JavaScript."));
		return TEXT("Error: V8 not initialized");
	}

	UE_LOG(LogJs, Verbose, TEXT("Executing JavaScript: %s"), *Script);

	// Enter isolate scope
	v8::Isolate::Scope isolate_scope(V8Isolate.get());
	v8::HandleScope handle_scope(V8Isolate.get());

	// Get Global context
	//v8::Global<v8::Context> *global = static_cast<v8::Global<v8::Context> *>(V8ContextGlobal);
	v8::Local<v8::Context> context = V8ContextGlobal.Get(V8Isolate.get());
	v8::Context::Scope context_scope(context);

	// Convert FString to UTF-8
	FTCHARToUTF8 Converter(*Script);
	const char* ScriptUTF8 = Converter.Get();

	// Create source string
	v8::MaybeLocal<v8::String> maybe_source =
		v8::String::NewFromUtf8(V8Isolate.get(), ScriptUTF8, v8::NewStringType::kNormal);

	v8::Local<v8::String> source;
	if (!maybe_source.ToLocal(&source))
	{
		UE_LOG(LogJs, Error, TEXT("Failed to create V8 source string"));
		return TEXT("Error: Failed to create source string");
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
		FString ErrorMsg = UTF8_TO_TCHAR(*exception_str);

		UE_LOG(LogJs, Error, TEXT("Failed to compile script: %s"), *ErrorMsg);
		return FString::Printf(TEXT("Error: Compilation failed - %s"), *ErrorMsg);
	}

	// Run script
	v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);

	v8::Local<v8::Value> result;
	if (!maybe_result.ToLocal(&result))
	{
		// Get exception message
		v8::Local<v8::Value> exception = try_catch.Exception();
		v8::String::Utf8Value exception_str(V8Isolate.get(), exception);
		FString ErrorMsg = UTF8_TO_TCHAR(*exception_str);

		UE_LOG(LogJs, Error, TEXT("Failed to execute script: %s"), *ErrorMsg);
		return FString::Printf(TEXT("Error: Execution failed - %s"), *ErrorMsg);
	}

	// Convert result to string
	v8::String::Utf8Value utf8(V8Isolate.get(), result);
	if (*utf8)
	{
		FString ResultString = UTF8_TO_TCHAR(*utf8);
		UE_LOG(LogJs, Log, TEXT("JavaScript executed successfully. Result: %s"), *ResultString);
		return ResultString;
	}
	else
	{
		UE_LOG(LogJs, Warning, TEXT("JavaScript executed but result is empty"));
		return TEXT("");
	}
}

void FV8Loader::LoadJsModule(const std::string_view ModuleName, FJsRuntime::FResolveModuleIdFn InResolve, FJsRuntime::FLoadSourceByModuleIdFn InLoadSource)
{
	if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
	{
		UE_LOG(LogJs, Error, TEXT("V8 is not initialized. Cannot load JS module."));
		return;
	}
	v8::Isolate* isolate = V8Isolate.get();
	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);

	v8::Local<v8::Context> ctx = V8ContextGlobal.Get(isolate);
	v8::Context::Scope context_scope(ctx);

	if (!JsModuleManager)
	{
		JsModuleManager = std::make_unique<FV8ModuleManager>(isolate, ctx);
	}

	UE_LOG(LogJs, Log, TEXT("Loading JS module: %s"), *FString(ModuleName.data()));
	JsModuleManager->LoadModule(ModuleName, InResolve, InLoadSource);

	v8::Local<v8::Value> outResult;
	JsModuleManager->ExcuteFunction(ModuleName, "hello", std::span<v8::Local<v8::Value>>(), outResult);

	if (!outResult.IsEmpty())
	{
		v8::Local<v8::String> s;
		if (outResult->ToString(ctx).ToLocal(&s))
		{
			v8::String::Utf8Value Utf8(isolate, s);
			const char* cstr = *Utf8 ? *Utf8 : "";
			UE_LOG(LogJs, Log, TEXT("ExcuteFunction: Call success. Return=%s"), *FString(UTF8_TO_TCHAR(cstr)));
		}
		else
		{
			UE_LOG(LogJs, Log, TEXT("ExcuteFunction: Call success. Return (non-string)."));
		}
	}
}

#undef LOCTEXT_NAMESPACE

