// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "V8ModuleManager.h"
#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif


class V8LOADER_API FV8LoaderModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Initialize V8 engine */
	void InitializeV8();
	
	/** Shutdown V8 engine */
	void ShutdownV8();
	
	/** Check if V8 is loaded and initialized */
	bool IsV8Loaded() const;

	/** Execute JavaScript code and return the result as a string */
	FString ExecuteJavaScript(const FString& Script);

	void LoadJsModule(const std::string& ModuleName);

private:
	/** V8 Platform */
	void* V8Platform;

	/** V8 Isolate */
	v8::Isolate* V8Isolate;
	v8::Global<v8::Context> V8ContextGlobal;

	/** Array buffer allocator */
	void* ArrayBufferAllocator;

	/** Flag to track if V8 is initialized */
	bool bIsInitialized;

	FV8ModuleManager* ModuleManager;
};
