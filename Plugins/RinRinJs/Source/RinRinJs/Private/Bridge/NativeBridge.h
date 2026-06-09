// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668)
#endif
#include "v8.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <memory>

class UWorld;

namespace rinrin::uejs
{
    class FActorHandleRegistry;

    /**
     * Injects the v0 `globalThis.ue` namespace into a V8 context.
     *
     * Lifetime:
     * - Owned by FScriptHost.
     * - Holds a TWeakObjectPtr<UWorld>; SetWorld() can change which world spawnActor*()
     *   operates on without rebuilding the V8 context.
     *
     * Threading: every JS call into UE happens on the game thread, and every UE call
     * into JS likewise. The bridge does not lock.
     */
    class FNativeBridge
    {
    public:
        FNativeBridge();
        ~FNativeBridge();

        FNativeBridge(const FNativeBridge &) = delete;
        FNativeBridge &operator=(const FNativeBridge &) = delete;

        void SetWorld(UWorld *World);
        UWorld *GetWorld() const { return WorldPtr.Get(); }

        void SetActorRegistry(FActorHandleRegistry *Registry) { ActorRegistry = Registry; }
        FActorHandleRegistry *GetActorRegistry() const { return ActorRegistry; }

        /** Build the `ue` object and attach it to `globalThis`. Must run inside Isolate+Context+Handle scopes. */
        void Inject(v8::Isolate *Isolate, v8::Local<v8::Context> Context);

    private:
        // --- JS callbacks (all signatures fixed by V8) ---
        static void Js_Log(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_SpawnActorByPath(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_Destroy(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_SetLocation(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_GetLocation(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_SetRotation(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_SetTransform(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_AddWorldOffset(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Js_SetVisible(const v8::FunctionCallbackInfo<v8::Value> &Args);

        // --- Helpers ---
        static FNativeBridge *FromArgs(const v8::FunctionCallbackInfo<v8::Value> &Args);
        static void Throw(v8::Isolate *Isolate, const char *Message);

        void BindFunction(v8::Isolate *Isolate,
                          v8::Local<v8::Context> Context,
                          v8::Local<v8::Object> Target,
                          const char *Name,
                          v8::FunctionCallback Cb);

        TWeakObjectPtr<UWorld> WorldPtr;
        FActorHandleRegistry *ActorRegistry = nullptr;
    };

} // namespace rinrin::uejs
