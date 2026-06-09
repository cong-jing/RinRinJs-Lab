// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/Expected.h"

#include <string>

namespace rinrin::uejs
{
    /**
     * Minimal v0 manifest. No semver, no deps, no permissions.
     * The manifest's only job is to point at one script package and its entry module.
     */
    struct FScriptManifest
    {
        FString PackageRootAbs; // absolute directory containing rinrin.manifest.json
        FString Name;
        FString Version;
        FString MainRelative; // e.g. "dist/main.js"

        FString MainAbs() const;
    };

    /** Reads `<PackageRootAbs>/rinrin.manifest.json` into FScriptManifest. */
    TExpected<FScriptManifest> LoadScriptManifest(const FString &PackageRootAbs);

} // namespace rinrin::uejs
