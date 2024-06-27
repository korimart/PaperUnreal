// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponentEx.h"
#include "Core/UECoroutine.h"
#include "Net/UnrealNetwork.h"
#include "TeamComponent.generated.h"


UCLASS()
class UTeamComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewTeamIndex, int32);
	FOnNewTeamIndex OnNewTeamIndex;
	
	TWeakAwaitable<int32> WaitForTeamIndex()
	{
		if (TeamIndex >= 0)
		{
			return TeamIndex;
		}
		
		return WaitForBroadcast(this, OnNewTeamIndex);
	}

	void SetTeamIndex(int32 NewIndex)
	{
		check(GetNetMode() != NM_Client);
		
		TeamIndex = NewIndex;
		
		if (GetNetMode() == NM_ListenServer)
		{
			OnRep_TeamIndex();
		}
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_TeamIndex)
	int32 TeamIndex = -1;

	UFUNCTION()
	void OnRep_TeamIndex()
	{
		OnNewTeamIndex.Broadcast(TeamIndex);
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		DOREPLIFETIME(ThisClass, TeamIndex);
	}
};
