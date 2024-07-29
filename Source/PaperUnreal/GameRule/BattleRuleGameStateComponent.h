// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentAttacherComponent.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
#include "BattleRuleGameStateComponent.generated.h"


UCLASS(Within=GameStateBase)
class UBattleRuleGameStateComponent : public UComponentAttacherComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UWorldTimerComponent* ServerWorldTimer;

	UPROPERTY()
	UAreaSpawnerComponent* ServerAreaSpawner;

	UPROPERTY()
	UPawnSpawnerComponent* ServerPawnSpawner;

private:
	virtual void AttachServerMachineComponents() override
	{
		ServerWorldTimer = NewChildComponent<UWorldTimerComponent>(GetOuterAGameStateBase());
		ServerWorldTimer->RegisterComponent();
		
		ServerAreaSpawner = NewChildComponent<UAreaSpawnerComponent>(GetOuterAGameStateBase());
		ServerAreaSpawner->RegisterComponent();
		
		ServerPawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOuterAGameStateBase());
		ServerPawnSpawner->RegisterComponent();
	}
};
