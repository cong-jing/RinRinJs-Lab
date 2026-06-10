// Copyright Epic Games, Inc. All Rights Reserved.

#include "NativeBridge.h"
#include "ActorHandleRegistry.h"
#include "Util/Log.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "UObject/UObjectGlobals.h"

#include <string>
#include <string_view>

namespace rinrin::uejs
{
    namespace
    {
        // --- value extraction helpers ---

        std::string ToUtf8(v8::Isolate *Isolate, v8::Local<v8::Value> Value)
        {
            if (Value.IsEmpty())
                return {};
            v8::Local<v8::Context> ctx = Isolate->GetCurrentContext();
            v8::Local<v8::String> s;
            if (!Value->ToString(ctx).ToLocal(&s))
                return {};
            v8::String::Utf8Value utf8(Isolate, s);
            return *utf8 ? std::string(*utf8, utf8.length()) : std::string();
        }

        bool ReadNumberMember(v8::Isolate *Isolate,
                              v8::Local<v8::Context> Context,
                              v8::Local<v8::Object> Obj,
                              const char *Key,
                              double DefaultValue,
                              double &OutValue)
        {
            v8::Local<v8::String> jsKey;
            if (!v8::String::NewFromUtf8(Isolate, Key, v8::NewStringType::kInternalized).ToLocal(&jsKey))
            {
                OutValue = DefaultValue;
                return false;
            }
            v8::Local<v8::Value> val;
            if (!Obj->Get(Context, jsKey).ToLocal(&val) || val->IsUndefined() || val->IsNull())
            {
                OutValue = DefaultValue;
                return true;
            }
            v8::Local<v8::Number> num;
            if (!val->ToNumber(Context).ToLocal(&num))
            {
                OutValue = DefaultValue;
                return false;
            }
            OutValue = num->Value();
            return true;
        }

        // ReadVector / ReadRotator semantics:
        //   - Value missing (empty / undefined / null)  -> OutVec = default, return true.
        //   - Value present and is a plain object       -> parse members; missing members
        //                                                   default to the matching field
        //                                                   of `DefaultValue`; non-numeric
        //                                                   members are an error.
        //   - Value present but is not an object        -> error (return false).
        // Returning false means "JS passed malformed data"; callers should throw.

        bool ReadVector(v8::Isolate *Isolate,
                        v8::Local<v8::Context> Context,
                        v8::Local<v8::Value> Value,
                        const FVector &DefaultValue,
                        FVector &OutVec)
        {
            OutVec = DefaultValue;
            if (Value.IsEmpty() || Value->IsUndefined() || Value->IsNull())
                return true;
            if (!Value->IsObject())
                return false;
            v8::Local<v8::Object> obj = Value.As<v8::Object>();
            double x, y, z;
            if (!ReadNumberMember(Isolate, Context, obj, "x", DefaultValue.X, x))
                return false;
            if (!ReadNumberMember(Isolate, Context, obj, "y", DefaultValue.Y, y))
                return false;
            if (!ReadNumberMember(Isolate, Context, obj, "z", DefaultValue.Z, z))
                return false;
            OutVec = FVector(x, y, z);
            return true;
        }

        bool ReadRotator(v8::Isolate *Isolate,
                         v8::Local<v8::Context> Context,
                         v8::Local<v8::Value> Value,
                         const FRotator &DefaultValue,
                         FRotator &OutRot)
        {
            OutRot = DefaultValue;
            if (Value.IsEmpty() || Value->IsUndefined() || Value->IsNull())
                return true;
            if (!Value->IsObject())
                return false;
            v8::Local<v8::Object> obj = Value.As<v8::Object>();
            double pitch, yaw, roll;
            if (!ReadNumberMember(Isolate, Context, obj, "pitch", DefaultValue.Pitch, pitch))
                return false;
            if (!ReadNumberMember(Isolate, Context, obj, "yaw", DefaultValue.Yaw, yaw))
                return false;
            if (!ReadNumberMember(Isolate, Context, obj, "roll", DefaultValue.Roll, roll))
                return false;
            OutRot = FRotator(pitch, yaw, roll);
            return true;
        }

        // ReadTransform: { location?: Vector, rotation?: Rotator, scale?: Vector }.
        // Each sub-field uses the same missing-vs-malformed rule as ReadVector/ReadRotator.
        // Returns false if any field is present but malformed; OutErrorField is then set
        // to the first offending field name ("location" / "rotation" / "scale" / "<root>").
        bool ReadTransform(v8::Isolate *Isolate,
                           v8::Local<v8::Context> Context,
                           v8::Local<v8::Value> Value,
                           FTransform &OutTransform,
                           const char *&OutErrorField)
        {
            OutTransform = FTransform::Identity;
            OutErrorField = nullptr;

            if (Value.IsEmpty() || Value->IsUndefined() || Value->IsNull())
                return true;
            if (!Value->IsObject())
            {
                OutErrorField = "<root>";
                return false;
            }

            v8::Local<v8::Object> obj = Value.As<v8::Object>();

            auto getMember = [&](const char *Key) -> v8::Local<v8::Value>
            {
                v8::Local<v8::String> jsKey;
                if (!v8::String::NewFromUtf8(Isolate, Key, v8::NewStringType::kInternalized).ToLocal(&jsKey))
                    return {};
                v8::Local<v8::Value> v;
                if (!obj->Get(Context, jsKey).ToLocal(&v))
                    return {};
                return v;
            };

            FVector loc = FVector::ZeroVector;
            FRotator rot = FRotator::ZeroRotator;
            FVector scale = FVector::OneVector;

            if (!ReadVector(Isolate, Context, getMember("location"), FVector::ZeroVector, loc))
            {
                OutErrorField = "location";
                return false;
            }
            if (!ReadRotator(Isolate, Context, getMember("rotation"), FRotator::ZeroRotator, rot))
            {
                OutErrorField = "rotation";
                return false;
            }
            if (!ReadVector(Isolate, Context, getMember("scale"), FVector::OneVector, scale))
            {
                OutErrorField = "scale";
                return false;
            }

            OutTransform = FTransform(rot.Quaternion(), loc, scale);
            return true;
        }

        v8::Local<v8::Object> MakeVector(v8::Isolate *Isolate, v8::Local<v8::Context> Context, const FVector &V)
        {
            v8::Local<v8::Object> obj = v8::Object::New(Isolate);
            auto set = [&](const char *Key, double Value)
            {
                v8::Local<v8::String> k;
                if (v8::String::NewFromUtf8(Isolate, Key, v8::NewStringType::kInternalized).ToLocal(&k))
                {
                    obj->Set(Context, k, v8::Number::New(Isolate, Value)).Check();
                }
            };
            set("x", V.X);
            set("y", V.Y);
            set("z", V.Z);
            return obj;
        }
    } // namespace

    FNativeBridge::FNativeBridge() = default;
    FNativeBridge::~FNativeBridge() = default;

    void FNativeBridge::SetWorld(UWorld *World)
    {
        WorldPtr = World;
    }

    void FNativeBridge::Throw(v8::Isolate *Isolate, const char *Message)
    {
        v8::Local<v8::String> msg;
        if (v8::String::NewFromUtf8(Isolate, Message ? Message : "rinrin native error",
                                    v8::NewStringType::kNormal)
                .ToLocal(&msg))
        {
            Isolate->ThrowException(v8::Exception::Error(msg));
        }
    }

    FNativeBridge *FNativeBridge::FromArgs(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Local<v8::Value> data = Args.Data();
        if (data.IsEmpty() || !data->IsExternal())
            return nullptr;
        return static_cast<FNativeBridge *>(data.As<v8::External>()->Value());
    }

    void FNativeBridge::BindFunction(v8::Isolate *Isolate,
                                     v8::Local<v8::Context> Context,
                                     v8::Local<v8::Object> Target,
                                     const char *Name,
                                     v8::FunctionCallback Cb)
    {
        v8::Local<v8::External> data = v8::External::New(Isolate, this);
        v8::Local<v8::Function> fn;
        if (!v8::Function::New(Context, Cb, data).ToLocal(&fn))
            return;
        v8::Local<v8::String> key;
        if (!v8::String::NewFromUtf8(Isolate, Name, v8::NewStringType::kInternalized).ToLocal(&key))
            return;
        Target->Set(Context, key, fn).Check();
    }

    void FNativeBridge::Inject(v8::Isolate *Isolate, v8::Local<v8::Context> Context)
    {
        v8::Context::Scope ctxScope(Context);

        v8::Local<v8::Object> global = Context->Global();
        v8::Local<v8::Object> ueObj = v8::Object::New(Isolate);

        BindFunction(Isolate, Context, ueObj, "log", &FNativeBridge::Js_Log);
        BindFunction(Isolate, Context, ueObj, "spawnActorByPath", &FNativeBridge::Js_SpawnActorByPath);
        BindFunction(Isolate, Context, ueObj, "destroy", &FNativeBridge::Js_Destroy);
        BindFunction(Isolate, Context, ueObj, "setLocation", &FNativeBridge::Js_SetLocation);
        BindFunction(Isolate, Context, ueObj, "getLocation", &FNativeBridge::Js_GetLocation);
        BindFunction(Isolate, Context, ueObj, "setRotation", &FNativeBridge::Js_SetRotation);
        BindFunction(Isolate, Context, ueObj, "setTransform", &FNativeBridge::Js_SetTransform);
        BindFunction(Isolate, Context, ueObj, "addWorldOffset", &FNativeBridge::Js_AddWorldOffset);
        BindFunction(Isolate, Context, ueObj, "setVisible", &FNativeBridge::Js_SetVisible);

        v8::Local<v8::String> ueKey;
        if (v8::String::NewFromUtf8(Isolate, "ue", v8::NewStringType::kInternalized).ToLocal(&ueKey))
        {
            global->Set(Context, ueKey, ueObj).Check();
        }
    }

    // ---------------- JS callbacks ----------------

    void FNativeBridge::Js_Log(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);

        FString combined;
        for (int i = 0; i < Args.Length(); ++i)
        {
            const std::string part = ToUtf8(isolate, Args[i]);
            if (i > 0)
                combined.AppendChar(TEXT(' '));
            combined.Append(UTF8_TO_TCHAR(part.c_str()));
        }

        UE_LOG(LogJs, Log, TEXT("[js] %s"), *combined);
    }

    void FNativeBridge::Js_SpawnActorByPath(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);

        FNativeBridge *self = FromArgs(Args);
        if (!self)
        {
            Throw(isolate, "ue.spawnActorByPath: bridge not bound");
            return;
        }
        if (Args.Length() < 1 || !Args[0]->IsString())
        {
            Throw(isolate, "ue.spawnActorByPath: first argument must be an asset path string");
            return;
        }

        UWorld *world = self->GetWorld();
        if (!world)
        {
            Throw(isolate, "ue.spawnActorByPath: no UWorld bound");
            return;
        }

        FActorHandleRegistry *registry = self->GetActorRegistry();
        if (!registry)
        {
            Throw(isolate, "ue.spawnActorByPath: actor registry not bound");
            return;
        }

        const std::string pathUtf8 = ToUtf8(isolate, Args[0]);
        if (pathUtf8.empty())
        {
            Throw(isolate, "ue.spawnActorByPath: empty asset path");
            return;
        }
        const FString assetPath = UTF8_TO_TCHAR(pathUtf8.c_str());

        UStaticMesh *mesh = LoadObject<UStaticMesh>(nullptr, *assetPath);
        if (!mesh)
        {
            const std::string msg = std::string("ue.spawnActorByPath: failed to load StaticMesh '") + pathUtf8 + "'";
            Throw(isolate, msg.c_str());
            return;
        }

        v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
        FTransform transform;
        if (Args.Length() >= 2)
        {
            const char *badField = nullptr;
            if (!ReadTransform(isolate, ctx, Args[1], transform, badField))
            {
                const std::string msg = std::string("ue.spawnActorByPath: invalid transform field '") +
                                        (badField ? badField : "?") + "'";
                Throw(isolate, msg.c_str());
                return;
            }
        }

        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        spawnParams.Name = MakeUniqueObjectName(
            world,
            AStaticMeshActor::StaticClass(),
            TEXT("RinRinJsDemoActor"));

        AStaticMeshActor *actor = world->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            transform,
            spawnParams);
        if (!actor)
        {
            Throw(isolate, "ue.spawnActorByPath: SpawnActor failed");
            return;
        }

        if (UStaticMeshComponent *smc = actor->GetStaticMeshComponent())
        {
            smc->SetMobility(EComponentMobility::Movable);
            smc->SetStaticMesh(mesh);
        }
        actor->SetActorScale3D(transform.GetScale3D());
#if WITH_EDITOR
        actor->SetActorLabel(TEXT("RinRinJsDemoActor"));
#endif

        const FActorHandleRegistry::FHandle handle = registry->Register(actor);
        Args.GetReturnValue().Set(v8::Integer::New(isolate, handle));
    }

    void FNativeBridge::Js_Destroy(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.destroy: bridge not bound");
            return;
        }
        if (Args.Length() < 1 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.destroy: handle must be an int");
            return;
        }
        FActorHandleRegistry::FHandle handle = Args[0].As<v8::Int32>()->Value();
        AActor *actor = self->GetActorRegistry()->Resolve(handle);
        if (actor && IsValid(actor))
        {
            actor->Destroy();
        }
        self->GetActorRegistry()->Forget(handle);
    }

    void FNativeBridge::Js_SetLocation(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.setLocation: bridge not bound");
            return;
        }
        if (Args.Length() < 2 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.setLocation: expected (handle, vector)");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.setLocation: invalid actor handle");
            return;
        }
        FVector loc;
        if (!ReadVector(isolate, isolate->GetCurrentContext(), Args[1], FVector::ZeroVector, loc))
        {
            Throw(isolate, "ue.setLocation: invalid vector");
            return;
        }
        actor->SetActorLocation(loc);
    }

    void FNativeBridge::Js_GetLocation(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.getLocation: bridge not bound");
            return;
        }
        if (Args.Length() < 1 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.getLocation: expected handle");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.getLocation: invalid actor handle");
            return;
        }
        Args.GetReturnValue().Set(MakeVector(isolate, isolate->GetCurrentContext(), actor->GetActorLocation()));
    }

    void FNativeBridge::Js_SetRotation(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.setRotation: bridge not bound");
            return;
        }
        if (Args.Length() < 2 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.setRotation: expected (handle, rotator)");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.setRotation: invalid actor handle");
            return;
        }
        FRotator rot;
        if (!ReadRotator(isolate, isolate->GetCurrentContext(), Args[1], FRotator::ZeroRotator, rot))
        {
            Throw(isolate, "ue.setRotation: invalid rotator");
            return;
        }
        actor->SetActorRotation(rot);
    }

    void FNativeBridge::Js_SetTransform(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.setTransform: bridge not bound");
            return;
        }
        if (Args.Length() < 2 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.setTransform: expected (handle, transform)");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.setTransform: invalid actor handle");
            return;
        }
        FTransform t;
        const char *badField = nullptr;
        if (!ReadTransform(isolate, isolate->GetCurrentContext(), Args[1], t, badField))
        {
            const std::string msg = std::string("ue.setTransform: invalid transform field '") +
                                    (badField ? badField : "?") + "'";
            Throw(isolate, msg.c_str());
            return;
        }
        actor->SetActorTransform(t);
    }

    void FNativeBridge::Js_AddWorldOffset(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.addWorldOffset: bridge not bound");
            return;
        }
        if (Args.Length() < 2 || !Args[0]->IsInt32())
        {
            Throw(isolate, "ue.addWorldOffset: expected (handle, vector)");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.addWorldOffset: invalid actor handle");
            return;
        }
        FVector offset;
        if (!ReadVector(isolate, isolate->GetCurrentContext(), Args[1], FVector::ZeroVector, offset))
        {
            Throw(isolate, "ue.addWorldOffset: invalid vector");
            return;
        }
        actor->AddActorWorldOffset(offset);
    }

    void FNativeBridge::Js_SetVisible(const v8::FunctionCallbackInfo<v8::Value> &Args)
    {
        v8::Isolate *isolate = Args.GetIsolate();
        v8::HandleScope hs(isolate);
        FNativeBridge *self = FromArgs(Args);
        if (!self || !self->GetActorRegistry())
        {
            Throw(isolate, "ue.setVisible: bridge not bound");
            return;
        }
        if (Args.Length() < 2 || !Args[0]->IsInt32() || !Args[1]->IsBoolean())
        {
            Throw(isolate, "ue.setVisible: expected (handle, bool)");
            return;
        }
        AActor *actor = self->GetActorRegistry()->Resolve(Args[0].As<v8::Int32>()->Value());
        if (!actor)
        {
            Throw(isolate, "ue.setVisible: invalid actor handle");
            return;
        }
        const bool visible = Args[1].As<v8::Boolean>()->Value();
        actor->SetActorHiddenInGame(!visible);
    }

} // namespace rinrin::uejs
