// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "Net/UnrealNetwork.h"
#include "TeamComponent.generated.h"


UCLASS()
class UTeamComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(TeamIndex);

private:
	UPROPERTY(ReplicatedUsing=OnRep_TeamIndex)
	int32 RepTeamIndex = -1;
	TLiveData<int32&> TeamIndex{RepTeamIndex};

	UFUNCTION()
	void OnRep_TeamIndex() { TeamIndex.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepTeamIndex);
	}

	UTeamComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};
