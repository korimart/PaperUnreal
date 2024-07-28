// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateTrackerComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
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
	UWorldTimerComponent* WorldTimerComponent;
	
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawnerComponent;
	
	UPROPERTY()
	UPawnSpawnerComponent* PawnSpawnerComponent;
	
	UPROPERTY()
	UReadyStateTrackerComponent* ReadyStateTrackerComponent;

private:
	APVPBattleGameState()
	{
		StageComponent = CreateDefaultSubobject<UStageComponent>(TEXT("StageComponent"));
		WorldTimerComponent = CreateDefaultSubobject<UWorldTimerComponent>(TEXT("WorldTimerComponent"));
		AreaSpawnerComponent = CreateDefaultSubobject<UAreaSpawnerComponent>(TEXT("AreaSpawnerComponent"));
		PawnSpawnerComponent = CreateDefaultSubobject<UPawnSpawnerComponent>(TEXT("PawnSpawnerComponent"));
		ReadyStateTrackerComponent = CreateDefaultSubobject<UReadyStateTrackerComponent>(TEXT("ReadyStateTrackerComponent"));
	}
};
