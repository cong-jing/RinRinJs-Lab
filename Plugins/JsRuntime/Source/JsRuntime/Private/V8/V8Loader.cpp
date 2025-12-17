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

namespace rinrin::jsruntime {

FV8Loader::FV8Loader() = default;
FV8Loader::~FV8Loader() = default;

static bool bProcessInitialized = false;

void FV8Loader::EnsureV8ProcessInitialized()
{
	// V8 requires process-wide initialization only once.
	if (bProcessInitialized) return;

	v8::V8::InitializeICUDefaultLocation(nullptr);
	v8::V8::InitializeExternalStartupData(nullptr);

	V8Platform = v8::platform::NewSingleThreadedDefaultPlatform();
	v8::V8::InitializePlatform(V8Platform.get());
	v8::V8::Initialize();

	bProcessInitialized = true;
}

void FV8Loader::FinalizeV8Process()
{
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
	V8Platform.reset();
	bProcessInitialized = false;
}

void FV8Loader::CreateExecutionContext()
{
	if (bIsInitialized)
	{
		UE_LOG(LogJs, Warning, TEXT("V8 already initialized"));
		return;
	}
	EnsureV8ProcessInitialized();

	JsModuleManager.reset();
	V8ContextGlobal.Reset();
	ArrayBufferAllocator.reset();
	V8Isolate.reset();

	UE_LOG(LogJs, Log, TEXT("Initializing V8 engine..."));

	// Step 3: Create Isolate
	v8::Isolate::CreateParams create_params;
	ArrayBufferAllocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
	create_params.array_buffer_allocator = ArrayBufferAllocator.get();

	UE_LOG(LogJs, Log, TEXT("Initializing V8 engine...  5555"));
	V8Isolate.reset(v8::Isolate::New(create_params));

	if (!V8Isolate)
	{
		UE_LOG(LogJs, Error, TEXT("Failed to create V8 Isolate"));
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("FV8Loader", "IsolateCreationError", "Failed to create V8 Isolate"));
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

void FV8Loader::DestroyExecutionContext()
{
	JsModuleManager.reset();
	V8ContextGlobal.Reset();
	ArrayBufferAllocator.reset();
	V8Isolate.reset();
	bIsInitialized = false;
}

bool FV8Loader::IsContextCreated() const
{
	return bIsInitialized && (V8Isolate.get() != nullptr);
}

std::string FV8Loader::ExecuteJavaScript(std::string_view ScriptUtf8)
{
	if (!bIsInitialized || !V8Isolate || V8ContextGlobal.IsEmpty())
	{
		UE_LOG(LogJs, Error, TEXT("V8 is not initialized. Cannot execute JavaScript."));
		return std::string("Error: V8 not initialized");
	}

	UE_LOG(LogJs, Verbose, TEXT("Executing JavaScript (len=%d)"), (int)ScriptUtf8.size());

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
		UE_LOG(LogJs, Error, TEXT("Failed to create V8 source string"));
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
		UE_LOG(LogJs, Error, TEXT("Failed to compile script: %s"), UTF8_TO_TCHAR(*exception_str));
		return std::string("Error: Compilation failed - ") + (*exception_str ? *exception_str : "");
	}

	// Run script
	v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);

	v8::Local<v8::Value> result;
	if (!maybe_result.ToLocal(&result))
	{
		// Get exception message
		v8::Local<v8::Value> exception = try_catch.Exception();
		v8::String::Utf8Value exception_str(V8Isolate.get(), exception);
		UE_LOG(LogJs, Error, TEXT("Failed to execute script: %s"), UTF8_TO_TCHAR(*exception_str));
		return std::string("Error: Execution failed - ") + (*exception_str ? *exception_str : "");
	}

	// Convert result to string (UTF-8)
	v8::String::Utf8Value utf8(V8Isolate.get(), result);
	if (*utf8)
	{
		UE_LOG(LogJs, Log, TEXT("JavaScript executed successfully. Result: %s"), UTF8_TO_TCHAR(*utf8));
		return std::string(*utf8);
	}
	else
	{
		UE_LOG(LogJs, Warning, TEXT("JavaScript executed but result is empty"));
		return std::string();
	}
}

void FV8Loader::LoadJsModule(const std::string_view ModuleName, FResolveModuleIdFn InResolve, FLoadSourceByModuleIdFn InLoadSource)
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
		JsModuleManager.reset(new FV8ModuleManager(isolate, ctx));
	}

	UE_LOG(LogJs, Log, TEXT("Loading JS module: %s"), *FString(ModuleName.data()));
	JsModuleManager->LoadModule(ModuleName, InResolve, InLoadSource);

	{
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
	{
		v8::Local<v8::Value> args[] = {
			v8::Integer::New(isolate, 10),
			v8::Integer::New(isolate, 20)
		};
		v8::Local<v8::Value> outResult;
		JsModuleManager->ExcuteFunction(ModuleName, "add", std::span(args), outResult);
		if (!outResult.IsEmpty())
		{
			v8::Local<v8::Number> s;
			if (outResult->ToNumber(ctx).ToLocal(&s))
			{
				UE_LOG(LogJs, Log, TEXT("ExcuteFunction: Call success. Return=%lf"), s->Value());
			}
			else
			{
				UE_LOG(LogJs, Log, TEXT("ExcuteFunction: Call success. Return (non-number)."));
			}
		}
	}
}

} // namespace rinrin::jsruntime

