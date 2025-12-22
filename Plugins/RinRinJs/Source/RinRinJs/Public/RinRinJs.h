// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleResolver.h"
#include "Common/Expected.h"
#include <string>

class RINRINJS_API FRinRinJsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	/** IModuleInterface implementation */
	virtual void ShutdownModule() override;

	void StartRuntime();
	void StopRuntime();

	rinrin::uejs::TExpected<void> LoadJsModule(const std::string_view ModuleName,
											   rinrin::uejs::FResolveModuleIdFn InResolve,
											   rinrin::uejs::FLoadSourceByModuleIdFn InLoadSource);
};
