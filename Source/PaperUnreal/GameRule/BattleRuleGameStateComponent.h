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
	virtual void AttachServerMachineComponents()
	{
		ServerWorldTimer = NewObject<UWorldTimerComponent>(GetOuterAGameStateBase());
		ServerWorldTimer->RegisterComponent();
		
		ServerAreaSpawner = NewObject<UAreaSpawnerComponent>(GetOuterAGameStateBase());
		ServerAreaSpawner->RegisterComponent();
		
		ServerPawnSpawner = NewObject<UPawnSpawnerComponent>(GetOuterAGameStateBase());
		ServerPawnSpawner->RegisterComponent();
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		DestroyIfValid(ServerWorldTimer);
		DestroyIfValid(ServerAreaSpawner);
		DestroyIfValid(ServerPawnSpawner);
	}
};
