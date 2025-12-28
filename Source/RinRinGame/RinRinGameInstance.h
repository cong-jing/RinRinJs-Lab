// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include <string>

#include "RinRinGameInstance.generated.h"

/**
 *
 */
UCLASS()
class RINRINGAME_API URinRinGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

private:
	static bool resolveModulePath(std::string_view ReferrerResolvedId, std::string_view RequestSpecifier, std::string &OutResolvedModuleId, std::string &OutError);
	static bool LoadJavascriptFile(std::string_view ResolvedModuleId, std::string &OutSourceUtf8, std::string &OutError);
};
