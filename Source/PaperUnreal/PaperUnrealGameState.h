// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "ResourceRegistryComponent.h"
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
	UResourceRegistryComponent* ResourceRegistryComponent;
	
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawnerComponent;

private:
	APaperUnrealGameState()
	{
		ResourceRegistryComponent = CreateDefaultSubobject<UResourceRegistryComponent>(TEXT("ResourceRegistryComponent"));
		AreaSpawnerComponent = CreateDefaultSubobject<UAreaSpawnerComponent>(TEXT("AreaSpawnerComponent"));
	}
};
