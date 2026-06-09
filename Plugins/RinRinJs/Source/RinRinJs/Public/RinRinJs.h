// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleResolver.h"
#include "Util/Expected.h"

#include <memory>
#include <string>

class UWorld;
struct IConsoleCommand;

namespace rinrin::uejs
{
	class FScriptHost;
}

class RINRINJS_API FRinRinJsModule : public IModuleInterface
{
public:
	FRinRinJsModule();
	virtual ~FRinRinJsModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void StartRuntime();
	void StopRuntime();

	rinrin::uejs::TExpected<void> LoadJsModule(const std::string_view ModuleName,
											   rinrin::uejs::FResolveModuleIdFn InResolve,
											   rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource);

	rinrin::uejs::TExpected<void> EvaluateString(const std::string_view ScriptUtf8);

	// ---- v0 script-package lifecycle ----

	/** Set the UWorld JS uses for spawn / actor calls. Safe to call repeatedly. */
	void SetGameWorld(UWorld *World);

	/** Load a script package (directory containing rinrin.manifest.json). Calls start() if exported. */
	rinrin::uejs::TExpected<void> LoadScriptPackage(const FString &PackageRootAbs);

	/** Unload current package: call dispose(), destroy JS-spawned actors, clear state. */
	void UnloadScriptPackage();

	/** Console-command target: dispose, rebuild V8 context, reload last package. */
	rinrin::uejs::TExpected<void> ReloadScriptPackage();

	/** Per-frame entry. Caller is responsible for ticking (e.g. GameInstance FTSTicker). */
	void TickRuntime(float DeltaSeconds);

	rinrin::uejs::FScriptHost *GetScriptHost() const { return ScriptHost.get(); }

private:
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	std::unique_ptr<rinrin::uejs::FScriptHost> ScriptHost;
	IConsoleCommand *ReloadCommand = nullptr;
};
