// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "StageComponent.generated.h"


UCLASS()
class UStageComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(CurrentStage);

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentStage)
	FName RepCurrentStage;
	mutable TLiveData<FName&> CurrentStage{RepCurrentStage};

	UFUNCTION()
	void OnRep_CurrentStage() { CurrentStage.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepCurrentStage);
	}

	UStageComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};
