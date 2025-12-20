// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JsRuntimeDefines.h"
#include <string>

class JSRUNTIME_API FJsRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	/** IModuleInterface implementation */
	virtual void ShutdownModule() override;

	void StartRuntime();
	void StopRuntime();

	void LoadJsModule(const std::string_view ModuleName,
		rinrin::jsruntime::FResolveModuleIdFn InResolve,
		rinrin::jsruntime::FLoadSourceByModuleIdFn InLoadSource);
};
