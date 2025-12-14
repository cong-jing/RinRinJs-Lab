// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Forward declarations for V8 types
namespace v8
{
	class Platform;
	class Isolate;
	class Context;
	template<class T> class Local;
	template<class T> class Global;  // Use Global instead of Persistent
}

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

private:
	/** V8 Platform */
	void* V8Platform;

	/** V8 Isolate */
	v8::Isolate* V8Isolate;

	/** V8 Context (Global handle) */
	void* V8ContextGlobal;

	/** Array buffer allocator */
	void* ArrayBufferAllocator;

	/** Flag to track if V8 is initialized */
	bool bIsInitialized;
};
