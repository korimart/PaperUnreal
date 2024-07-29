// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateTrackerComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PVPBattleGameState.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattleGameState : public AGameStateBase2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UStageComponent* StageComponent;
	
	UPROPERTY()
	UReadyStateTrackerComponent* ReadyStateTrackerComponent;

private:
	APVPBattleGameState()
	{
		StageComponent = CreateDefaultSubobject<UStageComponent>(TEXT("StageComponent"));
		ReadyStateTrackerComponent = CreateDefaultSubobject<UReadyStateTrackerComponent>(TEXT("ReadyStateTrackerComponent"));
	}
};
