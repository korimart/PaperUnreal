// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "ReadyStateComponent.h"
#include "TeamComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnrealPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class APaperUnrealPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UReadyStateComponent* ReadyStateComponent;
	
	UPROPERTY()
	UTeamComponent* TeamComponent;
	
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

private:
	APaperUnrealPlayerState()
	{
		ReadyStateComponent = CreateDefaultSubobject<UReadyStateComponent>(TEXT("ReadyStateComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
		InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	}
};
