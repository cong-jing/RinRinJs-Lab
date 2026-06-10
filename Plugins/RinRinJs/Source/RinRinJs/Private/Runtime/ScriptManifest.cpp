// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptManifest.h"
#include "Util/Log.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace rinrin::uejs
{

    FString FScriptManifest::MainAbs() const
    {
        FString result = FPaths::Combine(PackageRootAbs, MainRelative);
        FPaths::CollapseRelativeDirectories(result);
        FPaths::MakeStandardFilename(result);
        return result;
    }

    TExpected<FScriptManifest> LoadScriptManifest(const FString &PackageRootAbs)
    {
        // Normalize the root once so prefix checks elsewhere are exact.
        FString rootNormalized = PackageRootAbs;
        FPaths::CollapseRelativeDirectories(rootNormalized);
        FPaths::MakeStandardFilename(rootNormalized);

        FString manifestPath = FPaths::Combine(rootNormalized, TEXT("rinrin.manifest.json"));
        FPaths::CollapseRelativeDirectories(manifestPath);
        FPaths::MakeStandardFilename(manifestPath);

        FString text;
        if (!FFileHelper::LoadFileToString(text, *manifestPath))
        {
            return UEJS_MAKE_ERROR("LoadScriptManifest: cannot read '{}'", TCHAR_TO_UTF8(*manifestPath));
        }

        TSharedPtr<FJsonObject> json;
        TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(text);
        if (!FJsonSerializer::Deserialize(reader, json) || !json.IsValid())
        {
            return UEJS_MAKE_ERROR("LoadScriptManifest: failed to parse JSON in '{}'", TCHAR_TO_UTF8(*manifestPath));
        }

        FScriptManifest m;
        m.PackageRootAbs = rootNormalized;
        json->TryGetStringField(TEXT("name"), m.Name);
        json->TryGetStringField(TEXT("version"), m.Version);

        if (!json->TryGetStringField(TEXT("main"), m.MainRelative) || m.MainRelative.IsEmpty())
        {
            return UEJS_MAKE_ERROR(
                "LoadScriptManifest: '{}' is missing required 'main' field",
                TCHAR_TO_UTF8(*manifestPath));
        }

        return m;
    }

} // namespace rinrin::uejs
