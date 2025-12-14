// Copyright Epic Games, Inc. All Rights Reserved.

#include "V8Loader.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// Disable specific warnings for V8 headers
#pragma warning(push)
#pragma warning(disable : 4668) // undefined macro in #if
// Include V8 headers
#include "v8.h"
#include "libplatform/libplatform.h"
#pragma warning(pop)

#include "V8Logger.h"

#define LOCTEXT_NAMESPACE "FV8LoaderModule"
DEFINE_LOG_CATEGORY(LogJsV8)

void FV8LoaderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory
	V8Platform = nullptr;
	V8Isolate = nullptr;
	V8ContextGlobal = nullptr;
	ArrayBufferAllocator = nullptr;
	bIsInitialized = false;

	UE_LOG(LogJsV8, Log, TEXT("V8Loader module started"));
}

void FV8LoaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module
	ShutdownV8();

	UE_LOG(LogJsV8, Log, TEXT("V8Loader module shutdown"));
}

void FV8LoaderModule::InitializeV8()
{
	if (bIsInitialized)
	{
		UE_LOG(LogJsV8, Warning, TEXT("V8 already initialized"));
		return;
	}

	UE_LOG(LogJsV8, Log, TEXT("Initializing V8 engine..."));

	// Step 1: Initialize ICU and external startup data
	v8::V8::InitializeICUDefaultLocation(nullptr);
	v8::V8::InitializeExternalStartupData(nullptr);

	// Step 2: Create platform
	std::unique_ptr<v8::Platform> platform = v8::platform::NewSingleThreadedDefaultPlatform();
	V8Platform = platform.release();

	v8::V8::InitializePlatform(static_cast<v8::Platform *>(V8Platform));
	v8::V8::Initialize();

	// Step 3: Create Isolate
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	ArrayBufferAllocator = create_params.array_buffer_allocator;

	V8Isolate = v8::Isolate::New(create_params);

	if (!V8Isolate)
	{
		UE_LOG(LogJsV8, Error, TEXT("Failed to create V8 Isolate"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("IsolateCreationError", "Failed to create V8 Isolate"));
		return;
	}

	// Step 4: Create a Global context handle
	{
		v8::Isolate::Scope isolate_scope(V8Isolate);
		v8::HandleScope handle_scope(V8Isolate);

		v8::Local<v8::Context> context = v8::Context::New(V8Isolate);

		// Store context as Global (persistent handle)
		V8ContextGlobal = new v8::Global<v8::Context>(V8Isolate, context);
	}

	bIsInitialized = true;
	UE_LOG(LogJsV8, Log, TEXT("V8 engine initialized successfully"));
}

void FV8LoaderModule::ShutdownV8()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogJsV8, Log, TEXT("Shutting down V8 engine..."));

	// Dispose context
	if (V8ContextGlobal)
	{
		v8::Global<v8::Context> *global = static_cast<v8::Global<v8::Context> *>(V8ContextGlobal);
		global->Reset();
		delete global;
		V8ContextGlobal = nullptr;
	}

	// Dispose isolate
	if (V8Isolate)
	{
		V8Isolate->Dispose();
		V8Isolate = nullptr;
		UE_LOG(LogJsV8, Log, TEXT("V8 Isolate disposed"));
	}

	// Delete array buffer allocator
	if (ArrayBufferAllocator)
	{
		delete static_cast<v8::ArrayBuffer::Allocator *>(ArrayBufferAllocator);
		ArrayBufferAllocator = nullptr;
	}

	// Shutdown V8
	v8::V8::Dispose();

	// Dispose platform
	if (V8Platform)
	{
		delete static_cast<v8::Platform *>(V8Platform);
		V8Platform = nullptr;
	}

	bIsInitialized = false;
	UE_LOG(LogJsV8, Log, TEXT("V8 engine shut down successfully"));
}

bool FV8LoaderModule::IsV8Loaded() const
{
	return bIsInitialized && (V8Isolate != nullptr);
}

FString FV8LoaderModule::ExecuteJavaScript(const FString &Script)
{
	if (!bIsInitialized || !V8Isolate || !V8ContextGlobal)
	{
		UE_LOG(LogJsV8, Error, TEXT("V8 is not initialized. Cannot execute JavaScript."));
		return TEXT("Error: V8 not initialized");
	}

	UE_LOG(LogJsV8, Verbose, TEXT("Executing JavaScript: %s"), *Script);

	// Enter isolate scope
	v8::Isolate::Scope isolate_scope(V8Isolate);
	v8::HandleScope handle_scope(V8Isolate);

	// Get Global context
	v8::Global<v8::Context> *global = static_cast<v8::Global<v8::Context> *>(V8ContextGlobal);
	v8::Local<v8::Context> context = v8::Local<v8::Context>::New(V8Isolate, *global);
	v8::Context::Scope context_scope(context);

	// Convert FString to UTF-8
	FTCHARToUTF8 Converter(*Script);
	const char *ScriptUTF8 = Converter.Get();

	// Create source string
	v8::MaybeLocal<v8::String> maybe_source =
		v8::String::NewFromUtf8(V8Isolate, ScriptUTF8, v8::NewStringType::kNormal);

	v8::Local<v8::String> source;
	if (!maybe_source.ToLocal(&source))
	{
		UE_LOG(LogJsV8, Error, TEXT("Failed to create V8 source string"));
		return TEXT("Error: Failed to create source string");
	}

	// Compile script
	v8::TryCatch try_catch(V8Isolate);
	v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(context, source);

	v8::Local<v8::Script> script;
	if (!maybe_script.ToLocal(&script))
	{
		// Get exception message
		v8::Local<v8::Value> exception = try_catch.Exception();
		v8::String::Utf8Value exception_str(V8Isolate, exception);
		FString ErrorMsg = UTF8_TO_TCHAR(*exception_str);

		UE_LOG(LogJsV8, Error, TEXT("Failed to compile script: %s"), *ErrorMsg);
		return FString::Printf(TEXT("Error: Compilation failed - %s"), *ErrorMsg);
	}

	// Run script
	v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);

	v8::Local<v8::Value> result;
	if (!maybe_result.ToLocal(&result))
	{
		// Get exception message
		v8::Local<v8::Value> exception = try_catch.Exception();
		v8::String::Utf8Value exception_str(V8Isolate, exception);
		FString ErrorMsg = UTF8_TO_TCHAR(*exception_str);

		UE_LOG(LogJsV8, Error, TEXT("Failed to execute script: %s"), *ErrorMsg);
		return FString::Printf(TEXT("Error: Execution failed - %s"), *ErrorMsg);
	}

	// Convert result to string
	v8::String::Utf8Value utf8(V8Isolate, result);
	if (*utf8)
	{
		FString ResultString = UTF8_TO_TCHAR(*utf8);
		UE_LOG(LogJsV8, Log, TEXT("JavaScript executed successfully. Result: %s"), *ResultString);
		return ResultString;
	}
	else
	{
		UE_LOG(LogJsV8, Warning, TEXT("JavaScript executed but result is empty"));
		return TEXT("");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FV8LoaderModule, V8Loader)
