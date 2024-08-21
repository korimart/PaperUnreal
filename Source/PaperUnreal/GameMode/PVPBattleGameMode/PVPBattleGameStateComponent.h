// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleGameStateComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/StageComponent.h"
#include "PVPBattleGameStateComponent.generated.h"

/**
 * 
 */
UCLASS(Within=GameStateBase2)
class UPVPBattleGameStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	TLiveDataView<UStageComponent*&> GetStageComponent() const
	{
		return StageComponent;
	}

	DECLARE_LIVE_DATA_GETTER_SETTER(BattleGameStateComponent);

private:
	UPROPERTY(ReplicatedUsing=OnRep_StageComponent)
	UStageComponent* RepStageComponent;
	TLiveData<UStageComponent*&> StageComponent{RepStageComponent};

	UFUNCTION()
	void OnRep_StageComponent() { StageComponent.Notify(); }
	
	UPROPERTY(ReplicatedUsing=OnRep_BattleGameStateComponent)
	UBattleGameStateComponent* RepBattleGameStateComponent;
	TLiveData<UBattleGameStateComponent*&> BattleGameStateComponent{RepBattleGameStateComponent};
	
	UFUNCTION()
	void OnRep_BattleGameStateComponent() { BattleGameStateComponent.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepStageComponent, COND_InitialOnly);
		DOREPLIFETIME(ThisClass, RepBattleGameStateComponent);
	}

	virtual void AttachServerMachineComponents() override
	{
		RepStageComponent = NewChildComponent<UStageComponent>();
		RepStageComponent->RegisterComponent();
	}
};
