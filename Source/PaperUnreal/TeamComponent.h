// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponentEx.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "Net/UnrealNetwork.h"
#include "TeamComponent.generated.h"


UCLASS()
class UTeamComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(int32, TeamIndex);

private:
	UPROPERTY(ReplicatedUsing=OnRep_TeamIndex)
	int32 ReppedTeamIndex = -1;

	UFUNCTION()
	void OnRep_TeamIndex()
	{
		TeamIndex = ReppedTeamIndex;
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, ReppedTeamIndex);
	}
};
