// Copyright Epic Games, Inc. All Rights Reserved.

#include "RinRinJs.h"
#include "Common/LogMacros.h"
#include "Modules/ModuleManager.h"
#if RinRinJs_USE_V8
#include "V8/V8Loader.h"
using rinrin::uejs::FV8Loader;
#else
#include "ChakraCoreLoader.h"
#endif

DEFINE_LOG_CATEGORY(LogJs)

void FRinRinJsModule::StartupModule()
{
#if RinRinJs_USE_V8
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.EnsureV8ProcessInitialized();
#else
#endif
}
void FRinRinJsModule::ShutdownModule()
{
	StopRuntime();
#if RinRinJs_USE_V8
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.FinalizeV8Process();
#else
#endif
}

void FRinRinJsModule::StartRuntime()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if RinRinJs_USE_V8
	// Initialize V8 through V8Loader module
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.CreateExecutionContext();

	// Test JavaScript execution
	if (V8Loader.IsContextCreated())
	{
		UEJS_LOG(LogJs, Log, TEXT("Using V8 JavaScript Engine"));

		// Execute the hello world script
		FString TestScript = TEXT("'Hello from V8, ' + (1 + 2)");
		{
			FTCHARToUTF8 Conv(*TestScript);
			std::string Result = V8Loader.ExecuteJavaScript(std::string_view(Conv.Get(), Conv.Length()));
			UEJS_LOG(LogJs, Log, TEXT("JavaScript Test Result: %s"), UTF8_TO_TCHAR(Result.c_str()));
		}

		// Execute another test script
		FString MathScript = TEXT("(() => { return 2 + 2; })()");
		{
			FTCHARToUTF8 Conv(*MathScript);
			std::string MathResult = V8Loader.ExecuteJavaScript(std::string_view(Conv.Get(), Conv.Length()));
			UEJS_LOG(LogJs, Log, TEXT("JavaScript Math Test Result: %s"), UTF8_TO_TCHAR(MathResult.c_str()));
		}
	}
	else
	{
		UEJS_LOG(LogJs, Error, TEXT("V8 is not loaded, cannot execute JavaScript"));
	}
#else
	// Initialize ChakraCore through ChakraCoreLoader module
	FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::LoadModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
	ChakraCoreLoader.InitializeChakraCore();

	// Test JavaScript execution
	if (ChakraCoreLoader.IsChakraCoreLoaded())
	{
		UEJS_LOG(LogJs, Log, TEXT("Using ChakraCore JavaScript Engine"));

		// Execute the hello world script from the official example
		FString TestScript = TEXT("(()=>{return 'Hello world!';})()");
		FString Result = ChakraCoreLoader.ExecuteJavaScript(TestScript);
		UEJS_LOG(LogJs, Log, TEXT("JavaScript Test Result: %s"), *Result);

		// Execute another test script
		FString MathScript = TEXT("(()=>{return 2 + 2;})()");
		FString MathResult = ChakraCoreLoader.ExecuteJavaScript(MathScript);
		UEJS_LOG(LogJs, Log, TEXT("JavaScript Math Test Result: %s"), *MathResult);
	}
	else
	{
		UEJS_LOG(LogJs, Error, TEXT("ChakraCore is not loaded, cannot execute JavaScript"));
	}
#endif
	UEJS_LOG(LogJs, Log, TEXT("RinRinJs module started"));
}

void FRinRinJsModule::StopRuntime()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if RinRinJs_USE_V8
	// Shutdown V8 through V8Loader module
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.DestroyExecutionContext();
#else
	// Shutdown ChakraCore through ChakraCoreLoader module
	if (FModuleManager::Get().IsModuleLoaded("ChakraCoreLoader"))
	{
		FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::GetModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
		ChakraCoreLoader.ShutdownChakraCore();
	}
#endif
	UEJS_LOG(LogJs, Log, TEXT("RinRinJs module shutdown"));
}

void FRinRinJsModule::LoadJsModule(const std::string_view ModuleName,
								   rinrin::uejs::FResolveModuleIdFn InResolve,
								   rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource)
{
	UEJS_LOG(LogJs, Log, TEXT("LoadJsModule called with module name: %s"), *FString(ModuleName.data()));
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.LoadJsModule(ModuleName, InResolve, InLoadSource);
}

IMPLEMENT_MODULE(FRinRinJsModule, RinRinJs)
