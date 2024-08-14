// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
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
	
private:
	UPROPERTY(ReplicatedUsing=OnRep_StageComponent)
	UStageComponent* RepStageComponent;
	mutable TLiveData<UStageComponent*&> StageComponent{RepStageComponent};

	UFUNCTION()
	void OnRep_StageComponent() { StageComponent.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);

		DOREPLIFETIME_CONDITION(ThisClass, RepStageComponent, COND_InitialOnly);
	}

	virtual void AttachServerMachineComponents() override
	{
		RepStageComponent = NewChildComponent<UStageComponent>();
		RepStageComponent->RegisterComponent();
	}
};
