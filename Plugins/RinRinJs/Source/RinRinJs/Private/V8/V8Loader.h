// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "JsRuntimeDefines.h"
#include "V8/V8ModuleManager.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "CoreMinimal.h"
#include <memory>
#include <string_view>
#include <string>

namespace rinrin::jsruntime {

class FV8ModuleManager;

class FV8Loader
{
public:
	static FV8Loader& Get()
	{
		static FV8Loader instance;
		return instance;
	}

	FV8Loader(const FV8Loader&) = delete;
	FV8Loader& operator=(const FV8Loader&) = delete;

	void EnsureV8ProcessInitialized();
	void FinalizeV8Process();

	/** Initialize V8 engine */
	void CreateExecutionContext();
	
	/** Shutdown V8 engine */
	void DestroyExecutionContext();
	
	/** Check if V8 is loaded and initialized */
	bool IsContextCreated() const;

	/** Execute JavaScript code and return the result as a UTF-8 string */
	std::string ExecuteJavaScript(std::string_view ScriptUtf8);

	void LoadJsModule(const std::string_view ModuleName,
		FResolveModuleIdFn InResolve,
		FLoadSourceByModuleIdFn InLoadSource);

private:
	FV8Loader();
	~FV8Loader();

	/** V8 Platform */
	std::unique_ptr<v8::Platform> V8Platform;

	/** Array buffer allocator */
	std::unique_ptr<v8::ArrayBuffer::Allocator> ArrayBufferAllocator;

	// Custom deleter for v8::Isolate so unique_ptr can Dispose() it
	struct FIsolateDeleter
	{
		void operator()(v8::Isolate* Iso) const { if (Iso) Iso->Dispose(); }
	};

	/** V8 Isolate */
	std::unique_ptr<v8::Isolate, FIsolateDeleter> V8Isolate;

	v8::Global<v8::Context> V8ContextGlobal;

	struct FV8ModuleManagerDeleter
	{
		void operator()(FV8ModuleManager* ModuleManager) const { if (ModuleManager) ModuleManager->UnloadAll(); }
	};
	std::unique_ptr<FV8ModuleManager, FV8ModuleManagerDeleter> JsModuleManager;

	/** Flag to track if V8 is initialized */
	bool bIsInitialized;
};

} // namespace rinrin::jsruntime
