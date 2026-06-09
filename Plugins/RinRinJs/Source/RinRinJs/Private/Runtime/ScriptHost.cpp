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
        return WorldPtr.Get();
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
            // 2) Relative to referrer's directory. v0 only supports relative paths.
            const FString referrer = StdStringViewToFString(ReferrerResolvedId);
            const FString baseDir = FPaths::GetPath(referrer);
            resolved = FPaths::Combine(baseDir, request);
        }

        FPaths::CollapseRelativeDirectories(resolved);
        FPaths::MakeStandardFilename(resolved);
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
        // Idempotency: if same package is requested, treat as reload.
        if (bLoaded)
        {
            Unload();
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
        Bridge->SetWorld(WorldPtr.Get());

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

        // Read manifest.
        TExpected<FScriptManifest> manifestResult = LoadScriptManifest(PackageRootAbs);
        if (!manifestResult)
        {
            return Err(std::move(manifestResult).TakeError().WithContext("Loading manifest", UEJS_HERE));
        }
        CurrentManifest = std::move(*manifestResult);
        CurrentPackageRoot = PackageRootAbs;

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
                callResult.Error().Log(LogJs, ELogVerbosity::Error);
                // Demoting to a soft failure: package is "loaded" but start errored.
                // We don't unload here because dispose still needs to run, and tick may
                // succeed even when start crashed (this matches Node.js dev-loop habits).
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

        const FString savedRoot = CurrentPackageRoot;
        Unload();

        // Rebuild the V8 context to drop module cache + any stale references.
        // This is the "v0 reload that is stable and easy to explain" path from the design doc.
        FV8Loader &Loader = FV8Loader::Get();
        Loader.DestroyExecutionContext();
        Loader.CreateExecutionContext();

        return LoadPackageInternal(savedRoot);
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
            // We deliberately do NOT propagate. Per v0 policy: log and keep ticking
            // so a single bad frame doesn't kill the whole demo.
            r.Error().Log(LogJs, ELogVerbosity::Error);
        }
    }

} // namespace rinrin::uejs
