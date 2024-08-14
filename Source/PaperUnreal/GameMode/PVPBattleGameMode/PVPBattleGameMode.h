// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/GameModeBase2.h"
#include "PVPBattleGameMode.generated.h"


UCLASS()
class APVPBattleGameMode : public AGameModeBase2
{
	GENERATED_BODY()

private:
	APVPBattleGameMode();

	virtual void BeginPlay() override;
};
