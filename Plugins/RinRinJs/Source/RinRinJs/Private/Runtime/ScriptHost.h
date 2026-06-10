// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ScriptManifest.h"
#include "Util/Expected.h"

#include <memory>
#include <string>

class UWorld;

namespace rinrin::uejs
{
    class FNativeBridge;
    class FActorHandleRegistry;

    /**
     * Owns one script package at a time and drives its start / tick / dispose lifecycle.
     *
     * Pipeline:
     *   LoadPackage(packageRoot)
     *     -> if a package is already loaded:
     *          Unload() + drop the V8 execution context + release native bindings + recreate it
     *     -> ensure V8 context exists
     *     -> create NativeBridge + ActorRegistry
     *     -> inject `ue` globals
     *     -> read manifest
     *     -> compile + evaluate main module
     *     -> call exported start({ packageName }); a thrown start() is a hard failure
     *        but bLoaded stays true so a subsequent Unload() can still call dispose()
     *
     *   Tick(dt) -> call exported tick(dt) if present (errors are logged, not propagated)
     *
     *   Unload()
     *     -> call exported dispose() if present
     *     -> destroy JS-spawned actors
     *     -> clear actor registry
     *     -> keep native bridge storage alive until the V8 context is destroyed
     *
     *   Reload() -> LoadPackage(CurrentPackageRoot)
     */
    class FScriptHost
    {
    public:
        FScriptHost();
        ~FScriptHost();

        FScriptHost(const FScriptHost &) = delete;
        FScriptHost &operator=(const FScriptHost &) = delete;

        /** UWorld used for actor spawn calls. Safe to call before Load(). */
        void SetWorld(UWorld *World);
        UWorld *GetWorld() const;

        /** True when a package is loaded and start() has run. */
        bool IsLoaded() const { return bLoaded; }

        const FString &GetLoadedPackageRoot() const { return CurrentPackageRoot; }

        /** Load (or replace) a package. Idempotent failures are reported via TExpected. */
        TExpected<void> LoadPackage(const FString &PackageRootAbs);

        /** Unload current package (dispose + destroy actors). No-op if nothing is loaded. */
        void Unload();

        /** Release native objects referenced by V8 callbacks. Call only after the V8 context is gone. */
        void ReleaseNativeStateAfterContextDestroyed();

        /** Unload + rebuild execution context + reload last package. */
        TExpected<void> Reload();

        /** Per-frame tick; forwards to JS tick(dt) when available. Errors are logged, not propagated. */
        void Tick(float DeltaSeconds);

    private:
        TExpected<void> LoadPackageInternal(const FString &PackageRootAbs);

        // Module resolver/loader callbacks for FV8ModuleManager.
        bool ResolveModuleId(std::string_view ReferrerResolvedId,
                             std::string_view RequestSpecifier,
                             std::string &OutResolvedModuleId,
                             std::string &OutError) const;
        bool LoadModuleSource(std::string_view ResolvedModuleId,
                              std::string &OutSourceUtf8,
                              std::string &OutError) const;

        std::unique_ptr<FNativeBridge> Bridge;
        std::unique_ptr<FActorHandleRegistry> ActorRegistry;

        UWorld *WorldPtr = nullptr;

        FString CurrentPackageRoot;
        FScriptManifest CurrentManifest;
        std::string MainModuleId; // resolved id used as cache key inside FV8ModuleManager

        bool bLoaded = false;
        bool bTickMissingLogged = false;
    };

} // namespace rinrin::uejs
