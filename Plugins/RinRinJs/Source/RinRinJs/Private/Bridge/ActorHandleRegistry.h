// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include <cstdint>
#include <unordered_map>

class AActor;

namespace rinrin::uejs
{
    /**
     * Opaque actor handle registry for v0 demo.
     *
     * - JS receives an int32 handle.
     * - C++ keeps a TWeakObjectPtr<AActor> so we never dangle.
     * - All actors handed out here are considered "JS-owned" for v0:
     *   on Reset() we destroy them in the world.
     */
    class FActorHandleRegistry
    {
    public:
        using FHandle = int32;
        static constexpr FHandle kInvalidHandle = 0;

        FActorHandleRegistry() = default;
        ~FActorHandleRegistry() = default;

        FActorHandleRegistry(const FActorHandleRegistry &) = delete;
        FActorHandleRegistry &operator=(const FActorHandleRegistry &) = delete;

        /** Register an actor that was just spawned by JS. */
        FHandle Register(AActor *Actor);

        /**
         * Resolve a handle to an actor pointer.
         * Returns nullptr when the handle is invalid or the actor has been destroyed.
         */
        AActor *Resolve(FHandle Handle) const;

        /** Forget a handle without destroying the actor. */
        void Forget(FHandle Handle);

        /**
         * Destroy every actor that is still alive in the registry, then clear the table.
         * Safe to call when the world has been torn down (it just no-ops on stale entries).
         */
        void DestroyAllAndReset();

    private:
        FHandle NextHandle = 1;
        std::unordered_map<FHandle, TWeakObjectPtr<AActor>> Entries;
    };

} // namespace rinrin::uejs
