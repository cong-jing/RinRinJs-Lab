#pragma once

#include "CoreMinimal.h"

#include "Package.h"
#include "PackageLoader.h"
#include "Util/Log.h"
#include "V8/V8Includes.h"
#include "Value/ValueFromJs.h"
#include "Value/ValueIntoJs.h"

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <functional>
#include <span>

namespace rinrin::uejs
{
    struct FPackageInfo
    {
        std::string PackageName;
        std::string PackageVersion;
        std::string PackagePath;
        std::string EntryScript;
        std::string ManifestPath;
        bool bIsLoaded = false;
        bool bLoadFailed = false;
        std::string LastError;
        int32_t LoadOrder = 0;
    };

    class FPackageRegistry
    {
    public:
        FPackageRegistry(std::unique_ptr<IPackageLoader> InPackageLoader)
            : PackageLoader(std::move(InPackageLoader))
        {
        }
        ~FPackageRegistry() { UnloadAll(); }

        void UnloadAll();

        // Directly add an already-constructed package info (marked as not loaded)
        TExpected<void> AddPackage(FPackageInfo Info);
        // Parse a manifest and add its package info (not loaded yet)
        TExpected<void> AddPackageFromManifest(std::string_view ManifestFilePath);
        // Discover manifests under each search root (each subfolder is a package) and add them
        TExpected<void> AddPackagesFromSearchRoots(std::span<std::string_view> SearchRoots);

        // Load one package immediately (used internally by bulk load)
        TExpected<FPackage> LoadPackage(FPackageInfo Info);
        // Load all packages that are currently not loaded yet (ordered by LoadOrder then name)
        TExpected<void> LoadPendingPackages();

    private:
        std::unique_ptr<IPackageLoader> PackageLoader;

        std::unordered_map<std::string, FPackageInfo> PackageCache;
        TExpected<void> GatherPackageInfo(std::span<std::string_view> SearchRoots);
    };

} // namespace rinrin::uejs