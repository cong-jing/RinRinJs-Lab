// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JsRuntimeDefines.h"
#include <string>

class JSRUNTIME_API FJsRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void ShutdownModule() override;

	void InitialzeRuntime();
	void StopRuntime();

	void LoadJsModule(const std::string_view ModuleName,
		FJsRuntime::FResolveModuleIdFn InResolve,
		FJsRuntime::FLoadSourceByModuleIdFn InLoadSource);
};
