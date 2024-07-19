// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "PlayerSpawnerComponent.h"
#include "ReadyStateTrackerComponent.h"
#include "WorldTimerComponent.h"
#include "Core/GameStateBase2.h"
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
		WorldTimerComponent = CreateDefaultSubobject<UWorldTimerComponent>(TEXT("WorldTimerComponent"));
		AreaSpawnerComponent = CreateDefaultSubobject<UAreaSpawnerComponent>(TEXT("AreaSpawnerComponent"));
		PlayerSpawnerComponent = CreateDefaultSubobject<UPlayerSpawnerComponent>(TEXT("PlayerSpawnerComponent"));
		ReadyStateTrackerComponent = CreateDefaultSubobject<UReadyStateTrackerComponent>(TEXT("ReadyStateTrackerComponent"));
	}
};
