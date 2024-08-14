// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PVPBattleGameMode.generated.h"


UCLASS()
class APVPBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	APVPBattleGameMode();

	virtual void BeginPlay() override;
};
