// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChakraCoreLoader.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "ChakraCore.h"

#define LOCTEXT_NAMESPACE "FChakraCoreLoaderModule"

void FChakraCoreLoaderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory
	ChakraCoreHandle = nullptr;
	bIsInitialized = false;
	JsRuntime = nullptr;
	JsContext = nullptr;
	CurrentSourceContext = 0;
	
	UE_LOG(LogTemp, Log, TEXT("ChakraCoreLoader module started"));
}

void FChakraCoreLoaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module
	ShutdownChakraCore();
	
	UE_LOG(LogTemp, Log, TEXT("ChakraCoreLoader module shutdown"));
}

void FChakraCoreLoaderModule::InitializeChakraCore()
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("ChakraCore already initialized"));
		return;
	}

	// Get the base directory of the JsRuntime plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("JsRuntime")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LibraryPath;
	FString DllDirectory;
	
#if PLATFORM_WINDOWS
	DllDirectory = FPaths::Combine(*BaseDir, TEXT("ThirdParty/ChakraCore/bin/Win64/Release"));
	UE_LOG(LogTemp, Log, TEXT("Runtime add ChakraCore DLL Directory: %s"), *DllDirectory);
	LibraryPath = FPaths::Combine(*DllDirectory, TEXT("ChakraCore.dll"));
	
	// Add the directory to the DLL search path so Windows can find ChakraCore.dll
	FPlatformProcess::PushDllDirectory(*DllDirectory);
#elif PLATFORM_MAC
	// LibraryPath = FPaths::Combine(*BaseDir, TEXT("ThirdParty/ChakraCore/bin/Mac/Release/libChakraCore.dylib"));
#elif PLATFORM_LINUX
	// LibraryPath = FPaths::Combine(*BaseDir, TEXT("ThirdParty/ChakraCore/bin/Linux/Release/libChakraCore.so"));
#endif // PLATFORM_WINDOWS

	ChakraCoreHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

#if PLATFORM_WINDOWS
	// Remove the directory from the DLL search path after loading
	FPlatformProcess::PopDllDirectory(*DllDirectory);
#endif

	if (ChakraCoreHandle)
	{
		// Create a ChakraCore runtime
		JsErrorCode ErrorCode = JsCreateRuntime(JsRuntimeAttributeNone, nullptr, (JsRuntimeHandle*)&JsRuntime);
		if (ErrorCode != JsNoError)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create ChakraCore runtime. Error code: %d"), ErrorCode);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RuntimeCreationError", "Failed to create ChakraCore runtime"));
			return;
		}

		// Create an execution context
		ErrorCode = JsCreateContext((JsRuntimeHandle)JsRuntime, (JsContextRef*)&JsContext);
		if (ErrorCode != JsNoError)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create ChakraCore context. Error code: %d"), ErrorCode);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ContextCreationError", "Failed to create ChakraCore context"));
			JsDisposeRuntime((JsRuntimeHandle)JsRuntime);
			JsRuntime = nullptr;
			return;
		}

		// Set the current execution context
		ErrorCode = JsSetCurrentContext((JsContextRef)JsContext);
		if (ErrorCode != JsNoError)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to set ChakraCore context. Error code: %d"), ErrorCode);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SetContextError", "Failed to set ChakraCore context"));
			JsDisposeRuntime((JsRuntimeHandle)JsRuntime);
			JsRuntime = nullptr;
			JsContext = nullptr;
			return;
		}

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("Successfully loaded ChakraCore library from: %s"), *LibraryPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load ChakraCore library from: %s"), *LibraryPath);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ThirdPartyLibraryError", "Failed to load ChakraCore library"));
	}
}

void FChakraCoreLoaderModule::ShutdownChakraCore()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Clear the current context
	if (JsContext)
	{
		JsSetCurrentContext(JS_INVALID_REFERENCE);
		JsContext = nullptr;
	}

	// Dispose runtime
	if (JsRuntime)
	{
		JsDisposeRuntime((JsRuntimeHandle)JsRuntime);
		JsRuntime = nullptr;
		UE_LOG(LogTemp, Log, TEXT("ChakraCore runtime disposed"));
	}

	// Free the dll handle
	if (ChakraCoreHandle)
	{
		FPlatformProcess::FreeDllHandle(ChakraCoreHandle);
		ChakraCoreHandle = nullptr;
		UE_LOG(LogTemp, Log, TEXT("ChakraCore library unloaded"));
	}
	
	bIsInitialized = false;
	CurrentSourceContext = 0;
}

bool FChakraCoreLoaderModule::IsChakraCoreLoaded() const
{
	return bIsInitialized && (ChakraCoreHandle != nullptr) && (JsRuntime != nullptr);
}

FString FChakraCoreLoaderModule::ExecuteJavaScript(const FString& Script)
{
	if (!bIsInitialized || !JsRuntime || !JsContext)
	{
		UE_LOG(LogTemp, Error, TEXT("ChakraCore is not initialized. Cannot execute JavaScript."));
		return TEXT("Error: ChakraCore not initialized");
	}

	UE_LOG(LogTemp, Log, TEXT("ExecuteJavaScript %s"), *Script);

	// Convert FString to wide string
	const wchar_t* ScriptWC = *Script;
	
	JsValueRef Result;
	JsErrorCode ErrorCode = JsRunScript(ScriptWC, CurrentSourceContext++, L"", &Result);
	
	if (ErrorCode != JsNoError)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to execute JavaScript. Error code: %d"), ErrorCode);
		return FString::Printf(TEXT("Error: Failed to execute script (Error code: %d)"), ErrorCode);
	}

	// Convert result to String in JavaScript
	JsValueRef ResultJSString;
	ErrorCode = JsConvertValueToString(Result, &ResultJSString);
	
	if (ErrorCode != JsNoError)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to convert result to string. Error code: %d"), ErrorCode);
		return FString::Printf(TEXT("Error: Failed to convert result (Error code: %d)"), ErrorCode);
	}

	// Project script result back to C++
	const wchar_t* ResultWC = nullptr;
	size_t StringLength = 0;
	ErrorCode = JsStringToPointer(ResultJSString, &ResultWC, &StringLength);
	
	if (ErrorCode != JsNoError || !ResultWC)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get string pointer. Error code: %d"), ErrorCode);
		return FString::Printf(TEXT("Error: Failed to get result string (Error code: %d)"), ErrorCode);
	}

	// Convert to FString
	FString ResultString(ResultWC);
	UE_LOG(LogTemp, Log, TEXT("JavaScript executed successfully. Result: %s"), *ResultString);
	
	return ResultString;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FChakraCoreLoaderModule, ChakraCoreLoader)
