// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/ModeAgnostic/PrivilegeComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/TeamComponent.h"
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
	
	UPROPERTY()
	UPrivilegeComponent* ServerPrivilegeComponent;

private:
	APVPBattlePlayerState()
	{
		ReadyStateComponent = CreateDefaultSubobject<UReadyStateComponent>(TEXT("ReadyStateComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
		InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	}

	virtual void PreInitializeComponents() override
	{
		Super::PreInitializeComponents();

		if (GetNetMode() != NM_Client)
		{
			ServerPrivilegeComponent = NewObject<UPrivilegeComponent>(this);
			ServerPrivilegeComponent->RegisterComponent();
		}
	}
};
