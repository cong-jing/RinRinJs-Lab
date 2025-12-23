// Fill out your copyright notice in the Description page of Project Settings.

#include "RinRinGameInstance.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "RinRinJs.h"

void URinRinGameInstance::Init()
{
	Super::Init();
	// Custom initialization code can be added here

	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance initialized"));

	// Get the JsRuntime module and initialize it
	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &JsModule = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");

		JsModule.StartRuntime();
		auto loadResult = JsModule.LoadJsModule("main", // Resolve module ID callback
												[](std::string_view ReferrerResolvedId, std::string_view RequestSpecifier, std::string &OutResolvedModuleId, std::string &OutError) -> bool
												{
			// Simple resolution logic: just return the request specifier as resolved ID
			UE_LOG(LogTemp, Log, TEXT("Resolving module: %s (referrer: %s)"), *FString(RequestSpecifier.data()), *FString(ReferrerResolvedId.data()));
			OutResolvedModuleId = std::string(RequestSpecifier);
			return true; },
												// Load source by module ID callback
												[](std::string_view ResolvedModuleId, std::string &OutSourceUtf8, std::string &OutError) -> bool
												{
			// Simple loading logic: for demonstration, return a hardcoded source
			UE_LOG(LogTemp, Log, TEXT("Loading script file: %s"), *FString(ResolvedModuleId.data()));
			if (ResolvedModuleId == "main")
			{
				OutSourceUtf8 = 
"import { bar } from './utils.js';"

"function foo(a, b) {"
"    return a + b;"
"}"

"export { foo, bar };";
				return true;
			}
			else if (ResolvedModuleId == "./utils.js")
			{
				OutSourceUtf8 = 
					"export function bar(x) {    return x * 2; ";
				return true;
			}
			else
			{
				OutError = "Module not found: " + std::string(ResolvedModuleId);
				return false;
			} });

		if (!loadResult)
		{
			loadResult.Error().Log(LogTemp, ELogVerbosity::Error);
		}
		UE_LOG(LogTemp, Log, TEXT("RinRinJs StartupModule called from GameInstance"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RinRinJs module is not loaded"));
	}
}

void URinRinGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance shutting down"));

	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &JsModule = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");
		JsModule.StopRuntime();
	}

	Super::Shutdown();
}