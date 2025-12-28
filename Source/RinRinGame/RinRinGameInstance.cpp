// Fill out your copyright notice in the Description page of Project Settings.

#include "RinRinGameInstance.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "RinRinJs.h"
#include <string>
#include <string_view>

void URinRinGameInstance::Init()
{
	Super::Init();
	// Custom initialization code can be added here

	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance initialized"));

	// Get the JsRuntime module and initialize it
	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &module = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");

		module.StartRuntime();
		auto loadResult = module.LoadJsModule("main", // Resolve module ID callback
											  &URinRinGameInstance::resolveModulePath,
											  &URinRinGameInstance::LoadJavascriptFile);

		if (!loadResult)
		{
			loadResult.Error().Log(LogTemp, ELogVerbosity::Error);
		}

		auto evaResult = module.EvaluateString("foo(2, 3)");
		if (!evaResult)
		{
			evaResult.Error().Log(LogTemp, ELogVerbosity::Error);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RinRinJs module is not loaded"));
	}
}

void URinRinGameInstance::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("RinRinGameInstance shutting down"));

	if (FModuleManager::Get().IsModuleLoaded("RinRinJs"))
	{
		FRinRinJsModule &JsModule = FModuleManager::GetModuleChecked<FRinRinJsModule>("RinRinJs");
		JsModule.StopRuntime();
	}

	Super::Shutdown();
}

bool URinRinGameInstance::resolveModulePath(std::string_view ReferrerResolvedId, std::string_view RequestSpecifier,
											std::string &OutResolvedModuleId, std::string &OutError)
{
	FString resolved;
	if (RequestSpecifier == "main")
	{
		resolved = FPaths::Combine(FPaths::ProjectContentDir(), "Mods/Core/main.js");
	}
	else
	{
		//  1) 以 referrer 的“目录”作为基准
		const FString baseDir = FPaths::GetPath(FString(UTF8_TO_TCHAR(std::string(ReferrerResolvedId).c_str())));
		const FString targetFile = FString(UTF8_TO_TCHAR(std::string(RequestSpecifier).c_str()));
		// // 2) 拼接
		resolved = FPaths::Combine(baseDir, targetFile);
	}
	FPaths::CollapseRelativeDirectories(resolved);
	FPaths::MakeStandardFilename(resolved);
	OutResolvedModuleId = TCHAR_TO_UTF8(*resolved);
	return true;
}

bool URinRinGameInstance::LoadJavascriptFile(std::string_view ResolvedModuleId, std::string &OutSourceUtf8,
											 std::string &OutError)
{
	// 1) UTF-8 view -> FString（不要求 NUL 结尾，不需要额外拷贝）
	FUTF8ToTCHAR Conv(ResolvedModuleId.data(), (int32)ResolvedModuleId.size());
	const FString Filename(Conv.Length(), Conv.Get());

	FString Result;
	if (!FFileHelper::LoadFileToString(Result, *Filename))
	{
		// 2) 失败信息：UE 这个 API 本身不返回详细错误码，只给 bool
		//    你需要自己补充“为什么失败”的诊断信息（常见是：不存在/无权限/目录）
		IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();

		if (!PF.FileExists(*Filename))
		{
			OutError = "Failed to load JS file (not found): " + std::string(ResolvedModuleId);
		}
		else if (!PF.IsReadOnly(*Filename) && PF.DirectoryExists(*Filename))
		{
			OutError = "Failed to load JS file (path is a directory): " + std::string(ResolvedModuleId);
		}
		else
		{
			OutError = "Failed to load JS file (cannot read): " + std::string(ResolvedModuleId);
		}
		return false;
	}

	// 3) FString -> UTF-8 std::string（不额外分配两次）
	FTCHARToUTF8 Utf8(*Result);
	OutSourceUtf8.assign(Utf8.Get(), Utf8.Length());
	OutError.clear();
	return true;
}