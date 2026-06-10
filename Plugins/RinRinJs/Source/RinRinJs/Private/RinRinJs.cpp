// Copyright Epic Games, Inc. All Rights Reserved.

#include "RinRinJs.h"
#include "Util/Log.h"
#include "Runtime/ScriptHost.h"

#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#if RinRinJs_USE_V8
#include "V8/V8Loader.h"
using rinrin::uejs::FV8Loader;
#else
#include "ChakraCoreLoader.h"
#endif

DEFINE_LOG_CATEGORY(LogJs)

FRinRinJsModule::FRinRinJsModule() = default;
FRinRinJsModule::~FRinRinJsModule() = default;

void FRinRinJsModule::StartupModule()
{
	// ELogVerbosity
	UE_SET_LOG_VERBOSITY(LogJs, Verbose);
	UE_SET_LOG_VERBOSITY(LogJsInspector, Verbose);
#if RinRinJs_USE_V8
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.EnsureV8ProcessInitialized();
#else
#endif

	RegisterConsoleCommands();
}
void FRinRinJsModule::ShutdownModule()
{
	UnregisterConsoleCommands();

	if (ScriptHost)
	{
		ScriptHost->Unload();
	}

#if RinRinJs_USE_V8
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.FinalizeV8Process();
#else
#endif

	if (ScriptHost)
	{
		ScriptHost->ReleaseNativeStateAfterContextDestroyed();
		ScriptHost.reset();
	}
}

void FRinRinJsModule::StartRuntime()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if RinRinJs_USE_V8
	// Initialize V8 through V8Loader module
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.CreateExecutionContext();

	if (V8Loader.IsContextCreated())
	{
		UEJS_LOG(LogJs, Log, "Using V8 JavaScript Engine");
	}
	else
	{
		UEJS_LOG(LogJs, Error, "V8 is not loaded, cannot execute JavaScript");
	}
#else
	// Initialize ChakraCore through ChakraCoreLoader module
	FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::LoadModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
	ChakraCoreLoader.InitializeChakraCore();
	if (ChakraCoreLoader.IsChakraCoreLoaded())
	{
		UEJS_LOG(LogJs, Log, "Using ChakraCore JavaScript Engine");
	}
	else
	{
		UEJS_LOG(LogJs, Error, "ChakraCore is not loaded, cannot execute JavaScript");
	}
#endif

	if (!ScriptHost)
	{
		ScriptHost = std::make_unique<rinrin::uejs::FScriptHost>();
	}

	UEJS_LOG(LogJs, Log, "RinRinJs module started");
}

void FRinRinJsModule::StopRuntime()
{
	if (ScriptHost)
	{
		ScriptHost->Unload();
	}

#if RinRinJs_USE_V8
	// Shutdown V8 through V8Loader module
	FV8Loader &V8Loader = FV8Loader::Get();
	V8Loader.DestroyExecutionContext();
	if (ScriptHost)
	{
		ScriptHost->ReleaseNativeStateAfterContextDestroyed();
		ScriptHost.reset();
	}
#else
	// Shutdown ChakraCore through ChakraCoreLoader module
	if (FModuleManager::Get().IsModuleLoaded("ChakraCoreLoader"))
	{
		FChakraCoreLoaderModule &ChakraCoreLoader = FModuleManager::GetModuleChecked<FChakraCoreLoaderModule>("ChakraCoreLoader");
		ChakraCoreLoader.ShutdownChakraCore();
	}
	if (ScriptHost)
	{
		ScriptHost->ReleaseNativeStateAfterContextDestroyed();
		ScriptHost.reset();
	}
#endif
	UEJS_LOG(LogJs, Log, "RinRinJs module shutdown");
}

rinrin::uejs::TExpected<void> FRinRinJsModule::LoadJsModule(const std::string_view ModuleName,
															rinrin::uejs::FResolveModuleIdFn InResolve,
															rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource)
{
	UEJS_LOG(LogJs, Log, "LoadJsModule called with module name: {}", ModuleName);
	FV8Loader &V8Loader = FV8Loader::Get();
	return V8Loader.LoadJsModule(ModuleName, InResolve, InLoadSource);
}

rinrin::uejs::TExpected<void> FRinRinJsModule::EvaluateString(const std::string_view ScriptUtf8)
{
	UEJS_LOG(LogJs, Log, "EvaluateString called");
	FV8Loader &V8Loader = FV8Loader::Get();
	std::string Result = V8Loader.ExecuteJavaScript(ScriptUtf8);
	UEJS_LOG(LogJs, Log, "EvaluateString result: {}", Result);
	return rinrin::uejs::TExpected<void>();
}

void FRinRinJsModule::SetGameWorld(UWorld *World)
{
	if (!ScriptHost)
	{
		ScriptHost = std::make_unique<rinrin::uejs::FScriptHost>();
	}
	ScriptHost->SetWorld(World);
}

rinrin::uejs::TExpected<void> FRinRinJsModule::LoadScriptPackage(const FString &PackageRootAbs)
{
	if (!ScriptHost)
	{
		ScriptHost = std::make_unique<rinrin::uejs::FScriptHost>();
	}
	return ScriptHost->LoadPackage(PackageRootAbs);
}

void FRinRinJsModule::UnloadScriptPackage()
{
	if (ScriptHost)
	{
		ScriptHost->Unload();
	}
}

rinrin::uejs::TExpected<void> FRinRinJsModule::ReloadScriptPackage()
{
	if (!ScriptHost)
	{
		return rinrin::uejs::Err(rinrin::uejs::FError(
			"ReloadScriptPackage: no script host (call LoadScriptPackage first).", UEJS_HERE));
	}
	return ScriptHost->Reload();
}

void FRinRinJsModule::TickRuntime(float DeltaSeconds)
{
	if (ScriptHost)
	{
		ScriptHost->Tick(DeltaSeconds);
	}
}

void FRinRinJsModule::RegisterConsoleCommands()
{
	if (ReloadCommand)
		return;

	ReloadCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("RinRinJs.Reload"),
		TEXT("Reload the active JavaScript script package: dispose -> rebuild V8 context -> reload main module -> start."),
		FConsoleCommandDelegate::CreateLambda([this]()
											  {
			UEJS_LOG(LogJs, Log, "RinRinJs.Reload: reload requested");
			auto r = this->ReloadScriptPackage();
			if (!r)
			{
				r.Error().Log(LogJs, ELogVerbosity::Error);
				UEJS_LOG(LogJs, Error, "RinRinJs.Reload: failed");
			}
			else
			{
				UEJS_LOG(LogJs, Log, "RinRinJs.Reload: success");
			} }),
		ECVF_Default);
}

void FRinRinJsModule::UnregisterConsoleCommands()
{
	if (ReloadCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ReloadCommand);
		ReloadCommand = nullptr;
	}
}

IMPLEMENT_MODULE(FRinRinJsModule, RinRinJs)
