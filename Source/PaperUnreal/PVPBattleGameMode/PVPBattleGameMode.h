// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PVPBattleGameMode.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPVPBattleGameMode, Log, All);


namespace PVPBattleStage
{
	inline FName WaitingForConfig{TEXT("PVPBattleStage_WaitingForConfig")};
	inline FName Result{TEXT("PVPBattleStage_Result")};
}


UCLASS()
class APVPBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	APVPBattleGameMode();

	virtual void BeginPlay() override;
};
