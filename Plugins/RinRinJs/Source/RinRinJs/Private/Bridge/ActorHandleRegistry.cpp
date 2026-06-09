// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorHandleRegistry.h"

#include "GameFramework/Actor.h"

namespace rinrin::uejs
{

    FActorHandleRegistry::FHandle FActorHandleRegistry::Register(AActor *Actor)
    {
        if (!Actor)
            return kInvalidHandle;

        const FHandle handle = NextHandle++;
        Entries.emplace(handle, TWeakObjectPtr<AActor>(Actor));
        return handle;
    }

    AActor *FActorHandleRegistry::Resolve(FHandle Handle) const
    {
        if (Handle == kInvalidHandle)
            return nullptr;
        auto it = Entries.find(Handle);
        if (it == Entries.end())
            return nullptr;
        return it->second.Get();
    }

    void FActorHandleRegistry::Forget(FHandle Handle)
    {
        if (Handle == kInvalidHandle)
            return;
        Entries.erase(Handle);
    }

    void FActorHandleRegistry::DestroyAllAndReset()
    {
        for (auto &kv : Entries)
        {
            if (AActor *actor = kv.second.Get())
            {
                if (IsValid(actor))
                {
                    actor->Destroy();
                }
            }
        }
        Entries.clear();
        // NextHandle keeps growing so old handles never alias new ones, which is
        // what we want even across reloads.
    }

} // namespace rinrin::uejs
