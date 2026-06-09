// Copyright Epic Games, Inc. All Rights Reserved.

#include "RinRinJs.h"
#include "Util/Log.h"
#include "Modules/ModuleManager.h"
#if RinRinJs_USE_V8
#include "V8/V8Runtime.h"
using rinrin::uejs::FV8Runtime;
#else
#include "ChakraCoreLoader.h"
#endif

DEFINE_LOG_CATEGORY(LogJs)

void FRinRinJsModule::StartupModule()
{
	UE_SET_LOG_VERBOSITY(LogJs, VeryVerbose);
	UE_SET_LOG_VERBOSITY(LogJsInspector, Verbose);
#if RinRinJs_USE_V8
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	V8Runtime.EnsureV8ProcessInitialized();
#else
#endif
}
void FRinRinJsModule::ShutdownModule()
{
#if RinRinJs_USE_V8
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	V8Runtime.FinalizeV8Process();
#else
#endif
}

void FRinRinJsModule::StartRuntime()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if RinRinJs_USE_V8
	// Initialize V8 through V8Runtime module
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	V8Runtime.CreateExecutionContext();

	// Test JavaScript execution
	if (V8Runtime.IsContextCreated())
	{
		UEJS_LOG(LogJs, Log, "Using V8 JavaScript Engine");

		// Execute the hello world script
		FString TestScript = TEXT("'Hello from V8, ' + (1 + 2)");
		{
			FTCHARToUTF8 Conv(*TestScript);
			auto Result = V8Runtime.EvaluateScript(std::string_view(Conv.Get(), Conv.Length()));
			if (Result.HasError())
			{
				UEJS_LOG(LogJs, Error, "JavaScript execution error: {}", Result.Error().GetMessage());
				return;
			}
			else
			{
				UEJS_LOG(LogJs, Log, "JavaScript Test Result: {}", Result.Value().ToString());
			}
		}
	}
	else
	{
		UEJS_LOG(LogJs, Error, "V8 is not loaded, cannot execute JavaScript");
	}
#else
	// Initialize ChakraCore through ChakraCoreLoader module
	FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::LoadModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
	ChakraCoreLoader.InitializeChakraCore();

	// Test JavaScript execution
	if (ChakraCoreLoader.IsChakraCoreLoaded())
	{
		UEJS_LOG(LogJs, Log, "Using ChakraCore JavaScript Engine");

		// Execute the hello world script from the official example
		FString TestScript = TEXT("(()=>{return 'Hello world!';})()");
		FString Result = ChakraCoreLoader.ExecuteJavaScript(TestScript);
		UEJS_LOG(LogJs, Log, "JavaScript Test Result: {}", TCHAR_TO_UTF8(*Result));

		// Execute another test script
		FString MathScript = TEXT("(()=>{return 2 + 2;})()");
		FString MathResult = ChakraCoreLoader.ExecuteJavaScript(MathScript);
		UEJS_LOG(LogJs, Log, "JavaScript Math Test Result: {}", TCHAR_TO_UTF8(*MathResult));
	}
	else
	{
		UEJS_LOG(LogJs, Error, "ChakraCore is not loaded, cannot execute JavaScript");
	}
#endif
	UEJS_LOG(LogJs, Log, "RinRinJs module started");
}

void FRinRinJsModule::StopRuntime()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if RinRinJs_USE_V8
	// Shutdown V8 through V8Runtime module
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	V8Runtime.DestroyExecutionContext();
#else
	// Shutdown ChakraCore through ChakraCoreLoader module
	if (FModuleManager::Get().IsModuleLoaded("ChakraCoreLoader"))
	{
		FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::GetModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
		ChakraCoreLoader.ShutdownChakraCore();
	}
#endif
	UEJS_LOG(LogJs, Log, "RinRinJs module shutdown");
}

rinrin::uejs::TExpected<void> FRinRinJsModule::LoadJsModule(const std::string_view ModuleName,
															rinrin::uejs::FResolveModuleIdFn InResolve,
															rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource)
{
	UEJS_LOG(LogJs, Log, "LoadJsModule called with module name: {}", ModuleName);
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	return V8Runtime.LoadJsModule(ModuleName, InResolve, InLoadSource);
}

rinrin::uejs::TExpected<void> FRinRinJsModule::EvaluateString(const std::string_view ScriptUtf8)
{
	UEJS_LOG(LogJs, Log, "EvaluateString called");
	FV8Runtime &V8Runtime = FV8Runtime::Get();
	auto Result = V8Runtime.EvaluateScript(ScriptUtf8);
	if (Result.HasError())
	{
		UEJS_LOG(LogJs, Error, "EvaluateString error: {}", Result.Error().GetMessage());
		return Err(Result.Error());
	}
	UEJS_LOG(LogJs, Log, "EvaluateString result: {}", Result.Value().ToString());
	return rinrin::uejs::TExpected<void>();
}

IMPLEMENT_MODULE(FRinRinJsModule, RinRinJs)
