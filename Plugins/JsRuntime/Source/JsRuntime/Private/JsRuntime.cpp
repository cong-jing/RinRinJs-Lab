// Copyright Epic Games, Inc. All Rights Reserved.

#include "JsRuntime.h"
#include "JsRuntimeLogger.h"
#include "Modules/ModuleManager.h"
#if JS_RUNTIME_V8
#include "V8/V8Loader.h"
#else
#include "ChakraCoreLoader.h"
#endif

#define LOCTEXT_NAMESPACE "FJsRuntimeModule"
DEFINE_LOG_CATEGORY(LogJs)

void FJsRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if JS_RUNTIME_V8
	// Initialize V8 through V8Loader module
	FV8Loader& V8Loader = FV8Loader::Get();
	V8Loader.InitializeV8();

	// Test JavaScript execution
	if (V8Loader.IsV8Loaded())
	{
		UE_LOG(LogJs, Log, TEXT("Using V8 JavaScript Engine"));

		// Execute the hello world script
		FString TestScript = TEXT("'Hello from V8, ' + (1 + 2)");
		FString Result = V8Loader.ExecuteJavaScript(TestScript);
		UE_LOG(LogJs, Log, TEXT("JavaScript Test Result: %s"), *Result);

		// Execute another test script
		FString MathScript = TEXT("(() => { return 2 + 2; })()");
		FString MathResult = V8Loader.ExecuteJavaScript(MathScript);
		UE_LOG(LogJs, Log, TEXT("JavaScript Math Test Result: %s"), *MathResult);
	}
	else
	{
		UE_LOG(LogJs, Error, TEXT("V8 is not loaded, cannot execute JavaScript"));
	}
#else
	// Initialize ChakraCore through ChakraCoreLoader module
	FChakraCoreLoaderModule& ChakraCoreLoader = FModuleManager::LoadModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
	ChakraCoreLoader.InitializeChakraCore();

	// Test JavaScript execution
	if (ChakraCoreLoader.IsChakraCoreLoaded())
	{
		UE_LOG(LogJs, Log, TEXT("Using ChakraCore JavaScript Engine"));

		// Execute the hello world script from the official example
		FString TestScript = TEXT("(()=>{return 'Hello world!';})()");
		FString Result = ChakraCoreLoader.ExecuteJavaScript(TestScript);
		UE_LOG(LogJs, Log, TEXT("JavaScript Test Result: %s"), *Result);

		// Execute another test script
		FString MathScript = TEXT("(()=>{return 2 + 2;})()");
		FString MathResult = ChakraCoreLoader.ExecuteJavaScript(MathScript);
		UE_LOG(LogJs, Log, TEXT("JavaScript Math Test Result: %s"), *MathResult);
	}
	else
	{
		UE_LOG(LogJs, Error, TEXT("ChakraCore is not loaded, cannot execute JavaScript"));
	}
#endif
	UE_LOG(LogJs, Log, TEXT("JsRuntime module started"));
}

void FJsRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if JS_RUNTIME_V8
	// Shutdown V8 through V8Loader module
	FV8Loader& V8Loader = FV8Loader::Get();
	V8Loader.ShutdownV8();
#else
	// Shutdown ChakraCore through ChakraCoreLoader module
	if (FModuleManager::Get().IsModuleLoaded("ChakraCoreLoader"))
	{
		FChakraCoreLoaderModule& ChakraCoreLoader = FModuleManager::GetModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
		ChakraCoreLoader.ShutdownChakraCore();
	}
#endif
	UE_LOG(LogJs, Log, TEXT("JsRuntime module shutdown"));
}

void FJsRuntimeModule::LoadJsModule(const std::string_view ModuleName,
	FJsRuntime::FResolveModuleIdFn InResolve, 
	FJsRuntime::FLoadSourceByModuleIdFn InLoadSource)
{
	UE_LOG(LogJs, Log, TEXT("LoadJsModule called with module name: %s"), *FString(ModuleName.data()));
	FV8Loader& V8Loader = FV8Loader::Get();
	V8Loader.LoadJsModule(ModuleName, InResolve, InLoadSource);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FJsRuntimeModule, JsRuntime)
