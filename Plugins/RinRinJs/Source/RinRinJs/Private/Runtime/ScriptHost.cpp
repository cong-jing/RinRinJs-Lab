// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptHost.h"

#include "Bridge/ActorHandleRegistry.h"
#include "Bridge/NativeBridge.h"
#include "V8/V8Loader.h"
#include "V8/V8ModuleManager.h"
#include "Util/Log.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace rinrin::uejs
{
    namespace
    {
        FString StdStringViewToFString(std::string_view View)
        {
            if (View.empty())
                return FString();
            FUTF8ToTCHAR conv(View.data(), (int32)View.size());
            return FString(conv.Length(), conv.Get());
        }

        std::string FStringToStdString(const FString &S)
        {
            FTCHARToUTF8 conv(*S);
            return std::string(conv.Get(), conv.Length());
        }
    } // namespace

    FScriptHost::FScriptHost() = default;

    FScriptHost::~FScriptHost()
    {
        Unload();
    }

    void FScriptHost::SetWorld(UWorld *World)
    {
        WorldPtr = World;
        if (Bridge)
        {
            Bridge->SetWorld(World);
        }
    }

    UWorld *FScriptHost::GetWorld() const
    {
        return WorldPtr;
    }

    bool FScriptHost::ResolveModuleId(std::string_view ReferrerResolvedId,
                                      std::string_view RequestSpecifier,
                                      std::string &OutResolvedModuleId,
                                      std::string &OutError) const
    {
        const FString request = StdStringViewToFString(RequestSpecifier);
        FString resolved;

        // 1) Entry: the main module is identified by exactly the manifest's main path.
        if (ReferrerResolvedId.empty())
        {
            resolved = CurrentManifest.MainAbs();
        }
        else
        {
            // 2) Relative to referrer's directory. The current package loader only
            // supports relative paths.
            const FString referrer = StdStringViewToFString(ReferrerResolvedId);
            const FString baseDir = FPaths::GetPath(referrer);
            resolved = FPaths::Combine(baseDir, request);
        }

        FPaths::CollapseRelativeDirectories(resolved);
        FPaths::MakeStandardFilename(resolved);

        // 3) Clamp to package root: do not let scripts read files outside the package.
        //    PackageRootAbs is already normalized by LoadScriptManifest.
        const FString &root = CurrentManifest.PackageRootAbs;
        if (!root.IsEmpty())
        {
            FString rootWithSep = root;
            if (!rootWithSep.EndsWith(TEXT("/")) && !rootWithSep.EndsWith(TEXT("\\")))
            {
                rootWithSep.AppendChar(TEXT('/'));
            }
            if (!resolved.StartsWith(rootWithSep))
            {
                OutError = std::string("ResolveModuleId: refusing to resolve '") +
                           std::string(RequestSpecifier) +
                           "' outside package root ('" +
                           FStringToStdString(resolved) + "')";
                return false;
            }
        }

        OutResolvedModuleId = FStringToStdString(resolved);
        OutError.clear();
        return true;
    }

    bool FScriptHost::LoadModuleSource(std::string_view ResolvedModuleId,
                                       std::string &OutSourceUtf8,
                                       std::string &OutError) const
    {
        const FString filename = StdStringViewToFString(ResolvedModuleId);
        FString text;
        if (!FFileHelper::LoadFileToString(text, *filename))
        {
            OutError = std::string("LoadModuleSource: cannot read '") + std::string(ResolvedModuleId) + "'";
            return false;
        }
        FTCHARToUTF8 conv(*text);
        OutSourceUtf8.assign(conv.Get(), conv.Length());
        OutError.clear();
        return true;
    }

    TExpected<void> FScriptHost::LoadPackage(const FString &PackageRootAbs)
    {
        // If a package is already loaded, do the full reload pipeline: dispose old
        // package, then drop the V8 execution context so the module cache and any
        // stale module/global state are gone before we load the new package.
        if (bLoaded)
        {
            Unload();

            FV8Loader &Loader = FV8Loader::Get();
            Loader.DestroyExecutionContext();
            Loader.CreateExecutionContext();
        }
        return LoadPackageInternal(PackageRootAbs);
    }

    TExpected<void> FScriptHost::LoadPackageInternal(const FString &PackageRootAbs)
    {
        FV8Loader &Loader = FV8Loader::Get();
        if (!Loader.IsContextCreated())
        {
            Loader.CreateExecutionContext();
        }
        if (!Loader.IsContextCreated())
        {
            return UEJS_MAKE_ERROR("ScriptHost::LoadPackage: V8 context could not be created.");
        }

        v8::Isolate *isolate = Loader.GetIsolate();
        if (!isolate)
        {
            return UEJS_MAKE_ERROR("ScriptHost::LoadPackage: V8 isolate is null.");
        }

        // Create the module manager up-front so we can inject globals into a known context.
        Loader.EnsureModuleManager();

        // Reset per-load state.
        ActorRegistry = std::make_unique<FActorHandleRegistry>();
        Bridge = std::make_unique<FNativeBridge>();
        Bridge->SetActorRegistry(ActorRegistry.get());
        Bridge->SetWorld(WorldPtr);

        // Inject native globals into the V8 context.
        {
            v8::Isolate::Scope iso(isolate);
            v8::HandleScope hs(isolate);
            v8::Local<v8::Context> ctx = Loader.GetContext();
            if (ctx.IsEmpty())
            {
                return UEJS_MAKE_ERROR("ScriptHost::LoadPackage: V8 context handle is empty.");
            }
            Bridge->Inject(isolate, ctx);
        }

        // Remember the requested root before reading files so Reload() can retry after
        // fixing a missing or invalid manifest.
        CurrentPackageRoot = PackageRootAbs;

        // Read manifest.
        TExpected<FScriptManifest> manifestResult = LoadScriptManifest(PackageRootAbs);
        if (!manifestResult)
        {
            return Err(std::move(manifestResult).TakeError().WithContext("Loading manifest", UEJS_HERE));
        }
        CurrentManifest = std::move(*manifestResult);

        // Verify the main file exists before we try to compile it (better error message).
        const FString mainAbs = CurrentManifest.MainAbs();
        if (!FPaths::FileExists(mainAbs))
        {
            return UEJS_MAKE_ERROR(
                "ScriptHost::LoadPackage: main file does not exist: '{}'",
                TCHAR_TO_UTF8(*mainAbs));
        }

        MainModuleId = FStringToStdString(mainAbs);

        // Compile + instantiate + evaluate.
        UEJS_RETURN_IF_ERROR(
            "ScriptHost::LoadPackage: loading main module",
            Loader.LoadJsModule(
                std::string_view(MainModuleId.data(), MainModuleId.size()),
                [this](std::string_view r, std::string_view s, std::string &out, std::string &err)
                {
                    return this->ResolveModuleId(r, s, out, err);
                },
                [this](std::string_view r, std::string &out, std::string &err)
                {
                    return this->LoadModuleSource(r, out, err);
                }));

        bLoaded = true;
        bTickMissingLogged = false;

        // Call optional start(context).
        FV8ModuleManager *mgr = Loader.GetModuleManager();
        if (!mgr)
        {
            return UEJS_MAKE_ERROR("ScriptHost::LoadPackage: module manager unavailable after load.");
        }

        if (mgr->HasExportedFunction(MainModuleId, "start"))
        {
            v8::Isolate::Scope iso(isolate);
            v8::HandleScope hs(isolate);
            v8::Local<v8::Context> ctx = Loader.GetContext();
            v8::Context::Scope cs(ctx);

            v8::Local<v8::Object> contextObj = v8::Object::New(isolate);
            {
                v8::Local<v8::String> k;
                if (v8::String::NewFromUtf8(isolate, "packageName", v8::NewStringType::kInternalized).ToLocal(&k))
                {
                    const std::string nameUtf8 = FStringToStdString(CurrentManifest.Name);
                    v8::Local<v8::String> nameStr;
                    if (v8::String::NewFromUtf8(isolate, nameUtf8.c_str(), v8::NewStringType::kNormal).ToLocal(&nameStr))
                    {
                        contextObj->Set(ctx, k, nameStr).Check();
                    }
                }
            }

            v8::Local<v8::Value> args[] = {contextObj};
            v8::Local<v8::Value> ret;
            std::span<v8::Local<v8::Value>> argSpan(args, 1);
            auto callResult = mgr->ExecuteFunction(MainModuleId, "start", argSpan, ret);
            if (!callResult)
            {
                // We keep bLoaded == true so the next Unload() can still call dispose()
                // and tear down whatever the partial start() managed to create. But the
                // caller (and the RinRinJs.Reload console command) must see this as a
                // hard failure: the demo is not in a usable state.
                return Err(std::move(callResult).TakeError().WithContext("ScriptHost::LoadPackage: start() failed", UEJS_HERE));
            }
        }
        else
        {
            UEJS_LOG(LogJs, Log, "ScriptHost: main module exports no 'start' function (skipping).");
        }

        UEJS_LOG(LogJs, Log, "ScriptHost: loaded package '{}' (main='{}')",
                 TCHAR_TO_UTF8(*CurrentManifest.Name),
                 MainModuleId);
        return TExpected<void>();
    }

    void FScriptHost::Unload()
    {
        FV8Loader &Loader = FV8Loader::Get();

        if (bLoaded && Loader.IsContextCreated() && !MainModuleId.empty())
        {
            FV8ModuleManager *mgr = Loader.GetModuleManager();
            if (mgr && mgr->HasExportedFunction(MainModuleId, "dispose"))
            {
                v8::Isolate *isolate = Loader.GetIsolate();
                if (isolate)
                {
                    v8::Isolate::Scope iso(isolate);
                    v8::HandleScope hs(isolate);
                    v8::Local<v8::Value> ret;
                    std::span<v8::Local<v8::Value>> argSpan;
                    auto disposeResult = mgr->ExecuteFunction(MainModuleId, "dispose", argSpan, ret);
                    if (!disposeResult)
                    {
                        disposeResult.Error().Log(LogJs, ELogVerbosity::Warning);
                    }
                }
            }
        }

        if (ActorRegistry)
        {
            ActorRegistry->DestroyAllAndReset();
        }

        bLoaded = false;
        bTickMissingLogged = false;
        MainModuleId.clear();
        CurrentManifest = FScriptManifest();
        // Keep CurrentPackageRoot so Reload() can use it.

        Bridge.reset();
        ActorRegistry.reset();
    }

    TExpected<void> FScriptHost::Reload()
    {
        if (CurrentPackageRoot.IsEmpty())
        {
            return UEJS_MAKE_ERROR("ScriptHost::Reload: no package has ever been loaded.");
        }

        // LoadPackage() already performs unload + V8 context rebuild + reload when a
        // package is currently loaded. Reload() is just "do that against the last root".
        return LoadPackage(CurrentPackageRoot);
    }

    void FScriptHost::Tick(float DeltaSeconds)
    {
        if (!bLoaded)
            return;

        FV8Loader &Loader = FV8Loader::Get();
        if (!Loader.IsContextCreated())
            return;

        FV8ModuleManager *mgr = Loader.GetModuleManager();
        if (!mgr)
            return;

        if (!mgr->HasExportedFunction(MainModuleId, "tick"))
        {
            if (!bTickMissingLogged)
            {
                UEJS_LOG(LogJs, Log, "ScriptHost: main module exports no 'tick' function (silenced).");
                bTickMissingLogged = true;
            }
            return;
        }

        v8::Isolate *isolate = Loader.GetIsolate();
        if (!isolate)
            return;

        v8::Isolate::Scope iso(isolate);
        v8::HandleScope hs(isolate);

        v8::Local<v8::Value> args[] = {
            v8::Number::New(isolate, (double)DeltaSeconds)};
        v8::Local<v8::Value> ret;
        std::span<v8::Local<v8::Value>> argSpan(args, 1);
        auto r = mgr->ExecuteFunction(MainModuleId, "tick", argSpan, ret);
        if (!r)
        {
            // Log and keep ticking so a single bad frame does not stop the host.
            r.Error().Log(LogJs, ELogVerbosity::Error);
        }
    }

} // namespace rinrin::uejs
