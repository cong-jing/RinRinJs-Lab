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

	virtual void OnWorldChanged(UWorld *OldWorld, UWorld *NewWorld) override;

private:
	bool TickScripts(float DeltaSeconds);

	FTSTicker::FDelegateHandle TickHandle;
};
