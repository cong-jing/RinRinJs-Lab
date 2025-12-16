// Fill out your copyright notice in the Description page of Project Settings.


#include "RinRinGameInstance.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "JsRuntime.h"

void URinRinGameInstance::Init()
{
	Super::Init();
	// Custom initialization code can be added here

	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance initialized"));

	// Get the JsRuntime module and initialize it
	if (FModuleManager::Get().IsModuleLoaded("JsRuntime"))
	{
		FJsRuntimeModule& JsRuntimeModule = FModuleManager::GetModuleChecked<FJsRuntimeModule>("JsRuntime");
		JsRuntimeModule.StartupModule();

		JsRuntimeModule.LoadJsModule("main", // Resolve module ID callback
			[](std::string_view ReferrerResolvedId,
				std::string_view RequestSpecifier,
				std::string& OutResolvedModuleId,
				std::string& OutError) -> bool
		{
			// Simple resolution logic: just return the request specifier as resolved ID
			OutResolvedModuleId = std::string(RequestSpecifier);
			return true;
		},
			// Load source by module ID callback
			[](std::string_view ResolvedModuleId,
				std::string& OutSourceUtf8,
				std::string& OutError) -> bool
		{
			// Simple loading logic: for demonstration, return a hardcoded source
			UE_LOG(LogTemp, Log, TEXT("Loading JS Module: %s"), *FString(ResolvedModuleId.data()));
			if (ResolvedModuleId == "main")
			{
				OutSourceUtf8 = "export function hello() { return 'Hello from example module!'; }";
				return true;
			}
			else
			{
				OutError = "Module not found: " + std::string(ResolvedModuleId);
				return false;
			}
		});
		UE_LOG(LogTemp, Log, TEXT("JsRuntime StartupModule called from GameInstance"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("JsRuntime module is not loaded"));
	}
}

void URinRinGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance shutting down"));

	// Shutdown JsRuntime module
	if (FModuleManager::Get().IsModuleLoaded("JsRuntime"))
	{
		FJsRuntimeModule& JsRuntimeModule = FModuleManager::GetModuleChecked<FJsRuntimeModule>("JsRuntime");
		JsRuntimeModule.ShutdownModule();
		UE_LOG(LogTemp, Log, TEXT("JsRuntime ShutdownModule called from GameInstance"));
	}

	Super::Shutdown();
}