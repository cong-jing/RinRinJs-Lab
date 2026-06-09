// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "ModuleResolver.h"
#include "V8/V8Includes.h"
#include "Package.h"
#include "Value/ValueFromJs.h"
#include "Value/ValueFromJsImpl.h"
#include "Value/ValueIntoJs.h"

#include <memory>
#include <string>
#include <string_view>

namespace rinrin::uejs
{

    class FPackage;
    class FV8DevToolsServer;
    namespace inspector
    {
        class FV8Inspector;
    }

    class FV8Runtime
    {
    public:
        static FV8Runtime &Get()
        {
            static FV8Runtime instance;
            return instance;
        }

        FV8Runtime(const FV8Runtime &) = delete;
        FV8Runtime &operator=(const FV8Runtime &) = delete;

        void EnsureV8ProcessInitialized();
        void FinalizeV8Process();

        /** Initialize V8 engine */
        void CreateExecutionContext();

        /** Shutdown V8 engine */
        void DestroyExecutionContext();

        /** Check if V8 is loaded and initialized */
        bool IsContextCreated() const;

        TExpected<FValueFromJs> EvaluateScript(std::string_view ScriptUtf8);

        TExpected<void> LoadJsModule(const std::string_view ModuleName,
                                     FResolveModuleIdFn InResolve,
                                     FLoadSourceByModuleIdFn InLoadSource);

        TExpected<FValueFromJs> ExecuteJsFunction(const std::string_view ObjectName,
                                                  const std::string_view FunctionName,
                                                  const std::span<FValueIntoJs> &Args);

        TExpected<FValueFromJs> ExecuteJsFunction(const std::string_view ModuleName,
                                                  const std::string_view ObjectName,
                                                  const std::string_view FunctionName,
                                                  const std::span<FValueIntoJs> &Args);

        /** Get the V8 Isolate */
        v8::Isolate *GetV8Isolate() const;

        /** Get the V8 Context */
        v8::Local<v8::Context> GetV8Context() const;

        /** Get the JS Module Manager */
        FPackage *GetJsModuleManager() const;

        /** Get the Inspector for debugging */
        inspector::FV8Inspector *GetInspector() const;

    private:
        FV8Runtime();
        ~FV8Runtime();

        /** V8 Platform */
        std::unique_ptr<v8::Platform> V8Platform;

        /** Array buffer allocator */
        std::unique_ptr<v8::ArrayBuffer::Allocator> ArrayBufferAllocator;

        // Custom deleter for v8::Isolate so unique_ptr can Dispose() it
        struct FIsolateDeleter
        {
            void operator()(v8::Isolate *Iso) const
            {
                if (Iso)
                    Iso->Dispose();
            }
        };

        /** V8 Isolate */
        std::unique_ptr<v8::Isolate, FIsolateDeleter> V8Isolate;

        v8::Global<v8::Context> V8ContextGlobal;

        std::unique_ptr<FPackage> Esm;

        /** Inspector Transport (WebSocket) for debugging */
        std::unique_ptr<inspector::FV8Inspector> Inspector;

        /** Flag to track if V8 is initialized */
        bool bIsInitialized;
    };

} // namespace rinrin::uejs
