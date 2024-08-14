// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PrivilegeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/TeamComponent.h"
#include "PVPBattlePlayerState.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattlePlayerState : public APlayerState
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
	APVPBattlePlayerState()
	{
		ReadyStateComponent = CreateDefaultSubobject<UReadyStateComponent>(TEXT("ReadyStateComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
		InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	}
};
