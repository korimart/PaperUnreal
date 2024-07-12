// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnrealGameState.generated.h"

/**
 * 
 */
UCLASS()
class APaperUnrealGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawnerComponent;

private:
	APaperUnrealGameState()
	{
		AreaSpawnerComponent = CreateDefaultSubobject<UAreaSpawnerComponent>(TEXT("AreaSpawnerComponent"));
	}
};
