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
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(FName, CurrentStage, RepCurrentStage);

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentStage)
	FName RepCurrentStage;

	UFUNCTION()
	void OnRep_CurrentStage() { CurrentStage.OnRep(); }

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
