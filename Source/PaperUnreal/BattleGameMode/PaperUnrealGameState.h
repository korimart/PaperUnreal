// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/ModeAgnostic/PlayerSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateTrackerComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
#include "PaperUnrealGameState.generated.h"

/**
 * 
 */
UCLASS()
class APaperUnrealGameState : public AGameStateBase2
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
	UPlayerSpawnerComponent* PlayerSpawnerComponent;
	
	UPROPERTY()
	UReadyStateTrackerComponent* ReadyStateTrackerComponent;

private:
	APaperUnrealGameState()
	{
		StageComponent = CreateDefaultSubobject<UStageComponent>(TEXT("StageComponent"));
		WorldTimerComponent = CreateDefaultSubobject<UWorldTimerComponent>(TEXT("WorldTimerComponent"));
		AreaSpawnerComponent = CreateDefaultSubobject<UAreaSpawnerComponent>(TEXT("AreaSpawnerComponent"));
		PlayerSpawnerComponent = CreateDefaultSubobject<UPlayerSpawnerComponent>(TEXT("PlayerSpawnerComponent"));
		ReadyStateTrackerComponent = CreateDefaultSubobject<UReadyStateTrackerComponent>(TEXT("ReadyStateTrackerComponent"));
	}
};
