// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class CHAKRACORELOADER_API FChakraCoreLoaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Initialize ChakraCore */
	void InitializeChakraCore();
	
	/** Shutdown ChakraCore */
	void ShutdownChakraCore();
	
	/** Check if ChakraCore is loaded */
	bool IsChakraCoreLoaded() const;

	/** Execute JavaScript code and return the result as a string */
	FString ExecuteJavaScript(const FString& Script);

private:
	/** Handle to the ChakraCore dll we will load */
	void* ChakraCoreHandle;
	
	/** Flag to track if ChakraCore is initialized */
	bool bIsInitialized;

	/** ChakraCore runtime handle */
	void* JsRuntime;

	/** ChakraCore context */
	void* JsContext;

	/** Source context counter for script execution */
	unsigned int CurrentSourceContext;
};
