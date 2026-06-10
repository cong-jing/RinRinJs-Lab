// Fill out your copyright notice in the Description page of Project Settings.

#include "RinRinJsLabGameInstance.h"

#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RinRinJs.h"

namespace
{
	FString GetDemoPackageRoot()
	{
		FString root = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Mods/Core"));
		FPaths::CollapseRelativeDirectories(root);
		FPaths::MakeStandardFilename(root);
		return root;
	}
}

void URinRinJsLabGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("RinRinJsLabGameInstance initialized"));

	if (!FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		UE_LOG(LogTemp, Warning, TEXT("RinRinJs module is not loaded"));
		return;
	}

	FRinRinJsModule &module = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");
	module.StartRuntime();
	module.SetGameWorld(GetWorld());
	bPackageLoadAttempted = false;
	bPackageLoaded = false;

	// Per-frame tick driver. The Inspector uses its own FTSTicker for message pumping,
	// so we register a separate ticker here just for JS lifecycle tick(dt).
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &URinRinJsLabGameInstance::TickScripts));
}

void URinRinJsLabGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("RinRinJsLabGameInstance shutting down"));

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &JsModule = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");
		JsModule.UnloadScriptPackage();
		JsModule.SetGameWorld(nullptr);
		JsModule.StopRuntime();
	}
	bPackageLoaded = false;
	bPackageLoadAttempted = false;

	Super::Shutdown();
}

bool URinRinJsLabGameInstance::TickScripts(float DeltaSeconds)
{
	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &JsModule = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");
		UWorld *World = GetWorld();
		// Refresh the world from a stable game-thread tick rather than world-change
		// callbacks. That keeps the v0 demo away from teardown/hot-reload edges.
		JsModule.SetGameWorld(World);

		if (!bPackageLoaded && !bPackageLoadAttempted && World && World->IsGameWorld() && World->HasBegunPlay())
		{
			bPackageLoadAttempted = true;

			const FString packageRoot = GetDemoPackageRoot();
			UE_LOG(LogTemp, Log, TEXT("Loading JS script package from: %s"), *packageRoot);

			auto loadResult = JsModule.LoadScriptPackage(packageRoot);
			if (!loadResult)
			{
				loadResult.Error().Log(LogTemp, ELogVerbosity::Error);
			}
			else
			{
				bPackageLoaded = true;
			}
		}

		if (bPackageLoaded)
		{
			JsModule.TickRuntime(DeltaSeconds);
		}
	}
	return true; // keep ticking
}
