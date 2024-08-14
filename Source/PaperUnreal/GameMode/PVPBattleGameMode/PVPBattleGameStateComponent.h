// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateTrackerComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/WorldTimerComponent.h"
#include "PVPBattleGameStateComponent.generated.h"

/**
 * 
 */
UCLASS()
class UPVPBattleGameStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UWorldTimerComponent* WorldTimerComponent;
	
	UPROPERTY()
	UStageComponent* StageComponent;
	
	UPROPERTY()
	UReadyStateTrackerComponent* ReadyStateTrackerComponent;

private:
	UPVPBattleGameStateComponent()
	{
		WorldTimerComponent = CreateDefaultSubobject<UWorldTimerComponent>(TEXT("WorldTimerComponent"));
		StageComponent = CreateDefaultSubobject<UStageComponent>(TEXT("StageComponent"));
		ReadyStateTrackerComponent = CreateDefaultSubobject<UReadyStateTrackerComponent>(TEXT("ReadyStateTrackerComponent"));
	}
};
