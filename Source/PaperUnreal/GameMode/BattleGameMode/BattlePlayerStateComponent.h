// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/TeamComponent.h"
#include "BattlePlayerStateComponent.generated.h"


UCLASS()
class UBattlePlayerStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTeamComponent* ServerTeamComponent;
	
	UPROPERTY()
	UInventoryComponent* ServerInventoryComponent;

private:
	virtual void AttachServerMachineComponents() override
	{
		ServerTeamComponent = NewChildComponent<UTeamComponent>();
		ServerTeamComponent->RegisterComponent();
		
		ServerInventoryComponent = NewChildComponent<UInventoryComponent>();
		ServerInventoryComponent->RegisterComponent();
	}
};
