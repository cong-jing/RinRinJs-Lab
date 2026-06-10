// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"

#include "RinRinJsLabGameInstance.generated.h"

/**
 *
 */
UCLASS()
class RINRINJSLAB_API URinRinJsLabGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

private:
	bool TickScripts(float DeltaSeconds);

	FTSTicker::FDelegateHandle TickHandle;
	bool bPackageLoadAttempted = false;
	bool bPackageLoaded = false;
};
