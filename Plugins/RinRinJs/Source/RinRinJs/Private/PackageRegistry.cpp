#include "PackageRegistry.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <algorithm>
#include <vector>
#include <utility>

namespace rinrin::uejs
{
    constexpr const TCHAR *kManifestFileName = TEXT("manifest.json");

    FString ToFullPath(std::string_view InPath)
    {
        FString Path = UTF8_TO_TCHAR(std::string(InPath).c_str());
        return FPaths::ConvertRelativePathToFull(Path);
    }

    TExpected<rinrin::uejs::FPackageInfo> ParseManifest(const FString &ManifestPath)
    {
        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
        {
            return UEJS_MAKE_ERROR("Failed to read manifest file '{}'.", TCHAR_TO_UTF8(*ManifestPath));
        }

        TSharedPtr<FJsonObject> RootObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            return UEJS_MAKE_ERROR("Failed to parse manifest json '{}'.", TCHAR_TO_UTF8(*ManifestPath));
        }

        auto RequireStringField = [&](const TCHAR *Key, std::string &Out) -> bool
        {
            FString Value;
            if (!RootObject->TryGetStringField(Key, Value) || Value.IsEmpty())
            {
                return false;
            }
            Out = TCHAR_TO_UTF8(*Value);
            return true;
        };

        rinrin::uejs::FPackageInfo Info;
        if (!RequireStringField(TEXT("name"), Info.PackageName))
        {
            return UEJS_MAKE_ERROR("Manifest '{}' missing field 'name'.", TCHAR_TO_UTF8(*ManifestPath));
        }
        if (!RequireStringField(TEXT("version"), Info.PackageVersion))
        {
            return UEJS_MAKE_ERROR("Manifest '{}' missing field 'version'.", TCHAR_TO_UTF8(*ManifestPath));
        }

        FString Entry;
        if (!RootObject->TryGetStringField(TEXT("entry"), Entry) || Entry.IsEmpty())
        {
            return UEJS_MAKE_ERROR("Manifest '{}' missing field 'entry'.", TCHAR_TO_UTF8(*ManifestPath));
        }

        FString BasePath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(ManifestPath));
        Info.PackagePath = TCHAR_TO_UTF8(*BasePath);

        FString EntryAbs = Entry;
        if (FPaths::IsRelative(EntryAbs))
        {
            EntryAbs = FPaths::Combine(BasePath, EntryAbs);
        }
        EntryAbs = FPaths::ConvertRelativePathToFull(EntryAbs);
        Info.EntryScript = TCHAR_TO_UTF8(*EntryAbs);

        int64 OrderValue = 0;
        if (RootObject->TryGetNumberField(TEXT("order"), OrderValue))
        {
            Info.LoadOrder = static_cast<int32_t>(OrderValue);
        }

        Info.ManifestPath = TCHAR_TO_UTF8(*ManifestPath);
        Info.bIsLoaded = false;
        Info.bLoadFailed = false;
        Info.LastError.clear();

        return Info;
    }

    void FPackageRegistry::UnloadAll()
    {
        PackageCache.clear();
    }

    static void ResetLoadState(FPackageInfo &Info)
    {
        Info.bIsLoaded = false;
        Info.bLoadFailed = false;
        Info.LastError.clear();
    }

    TExpected<void> FPackageRegistry::AddPackage(FPackageInfo Info)
    {
        ResetLoadState(Info);
        auto InsertResult = PackageCache.emplace(Info.PackageName, Info);
        if (!InsertResult.second)
        {
            UEJS_LOG(LogJs, Warning, "Package '{}' info overwritten.", Info.PackageName);
            InsertResult.first->second = Info;
        }
        return TExpected<void>();
    }

    TExpected<void> FPackageRegistry::AddPackageFromManifest(std::string_view ManifestFilePath)
    {
        FString ManifestPath = ToFullPath(ManifestFilePath);
        if (!FPaths::FileExists(ManifestPath))
        {
            return UEJS_MAKE_ERROR("Manifest file not found '{}'.", TCHAR_TO_UTF8(*ManifestPath));
        }

        TExpected<FPackageInfo> Parsed = ParseManifest(ManifestPath);
        if (Parsed.HasError())
        {
            return Err(Parsed.Error());
        }

        FPackageInfo Info = Parsed.Value();
        ResetLoadState(Info);
        return AddPackage(std::move(Info));
    }

    TExpected<void> FPackageRegistry::AddPackagesFromSearchRoots(std::span<std::string_view> SearchRoots)
    {
        bool bHasError = false;

        for (std::string_view RootSv : SearchRoots)
        {
            FString RootPath = ToFullPath(RootSv);
            if (!IFileManager::Get().DirectoryExists(*RootPath))
            {
                UEJS_LOG(LogJs, Warning, "Search root '{}' does not exist, skipping.", TCHAR_TO_UTF8(*RootPath));
                continue;
            }

            IFileManager::Get().IterateDirectory(*RootPath, [&, this](const TCHAR *Path, bool bIsDirectory)
                                                 {
                if (!bIsDirectory)
                {
                    return true; // keep iterating
                }

                FString PackageFolder(Path);
                FString ManifestPath = FPaths::Combine(PackageFolder, kManifestFileName);
                if (!FPaths::FileExists(ManifestPath))
                {
                    UEJS_LOG(LogJs, VeryVerbose, "No manifest.json in subfolder '{}', skipping.", TCHAR_TO_UTF8(*PackageFolder));
                    return true;
                }

                TExpected<FPackageInfo> Parsed = ParseManifest(ManifestPath);
                if (Parsed.HasError())
                {
                    bHasError = true;
                    Parsed.Error().Log(LogJs, ELogVerbosity::Error);
                    return true;
                }

                FPackageInfo Info = Parsed.Value();
                ResetLoadState(Info);
                auto InsertResult = PackageCache.emplace(Info.PackageName, Info);
                if (!InsertResult.second)
                {
                    UEJS_LOG(LogJs, Warning, "Manifest for '{}' is overwritten by '{}'.", Info.PackageName, TCHAR_TO_UTF8(*ManifestPath));
                    InsertResult.first->second = Info;
                }

                UEJS_LOG(LogJs, Log, "Found package '{}' order={} entry='{}'.", Info.PackageName, Info.LoadOrder, Info.EntryScript);
                return true; });
        }

        if (bHasError)
        {
            return UEJS_MAKE_ERROR("One or more manifests failed to parse.");
        }

        return TExpected<void>();
    }

    TExpected<FPackage> FPackageRegistry::LoadPackage(FPackageInfo Info)
    {
        return FPackage(Isolate, Context.Get(Isolate), std::move(Info));
    }

    TExpected<void> FPackageRegistry::LoadPendingPackages()
    {
        std::vector<std::reference_wrapper<FPackageInfo>> Pending;
        Pending.reserve(PackageCache.size());
        for (auto &kv : PackageCache)
        {
            if (!kv.second.bIsLoaded)
            {
                Pending.emplace_back(kv.second);
            }
        }

        std::sort(Pending.begin(), Pending.end(), [](const auto &A, const auto &B)
                  {
            if (A.get().LoadOrder == B.get().LoadOrder)
            {
                return A.get().PackageName < B.get().PackageName;
            }
            return A.get().LoadOrder < B.get().LoadOrder; });

        bool bHasError = false;
        FError FirstError;

        for (FPackageInfo &Info : Pending)
        {
            UEJS_LOG(LogJs, Log, "Loading package '{}' order={} entry='{}'.", Info.PackageName, Info.LoadOrder, Info.EntryScript);

            TExpected<FPackage> Loaded = LoadPackage(Info);
            if (Loaded.HasError())
            {
                Info.bIsLoaded = false;
                Info.bLoadFailed = true;
                Info.LastError = Loaded.Error().GetMessage();
                Loaded.Error().Log(LogJs, ELogVerbosity::Error);
                if (!FirstError.IsValid())
                {
                    FirstError = Loaded.Error();
                }
                bHasError = true;
                continue;
            }

            Info.bIsLoaded = true;
            Info.bLoadFailed = false;
            Info.LastError.clear();
        }

        if (bHasError)
        {
            return Err(FirstError);
        }
        return TExpected<void>();
    }

    // Backward compat: keep GatherPackageInfo for existing callers (wrap AddPackagesFromSearchRoots)
    TExpected<void> FPackageRegistry::GatherPackageInfo(std::span<std::string_view> SearchRoots)
    {
        return AddPackagesFromSearchRoots(SearchRoots);
    }

} // namespace rinrin::uejs