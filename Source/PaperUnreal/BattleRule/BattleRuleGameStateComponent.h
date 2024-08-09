// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
#include "BattleRuleGameStateComponent.generated.h"


UCLASS(Within=GameStateBase)
class UBattleRuleGameStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UWorldTimerComponent* ServerWorldTimer;

	UPROPERTY()
	UPawnSpawnerComponent* ServerPawnSpawner;

	DECLARE_LIVE_DATA_GETTER_SETTER(AreaSpawner);

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaSpawner)
	UAreaSpawnerComponent* _;
	mutable TLiveData<UAreaSpawnerComponent*&> AreaSpawner{_};

	UFUNCTION()
	void OnRep_AreaSpawner() { AreaSpawner.Notify(); }
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, _, COND_InitialOnly);
	}
	
	virtual void AttachServerMachineComponents() override
	{
		ServerWorldTimer = NewChildComponent<UWorldTimerComponent>(GetOuterAGameStateBase());
		ServerWorldTimer->RegisterComponent();
		
		AreaSpawner = NewChildComponent<UAreaSpawnerComponent>(GetOuterAGameStateBase());
		AreaSpawner.Get()->RegisterComponent();
		
		ServerPawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOuterAGameStateBase());
		ServerPawnSpawner->RegisterComponent();
	}
};
