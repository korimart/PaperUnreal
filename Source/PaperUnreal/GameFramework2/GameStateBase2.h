// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "GameStateBase2.generated.h"

/**
 * 
 */
UCLASS()
class AGameStateBase2 : public AGameStateBase
{
	GENERATED_BODY()

	TBackedLiveData<
		TArray<TObjectPtr<APlayerState>>,
		ERepHandlingPolicy::CompareForAddOrRemove
	> PlayerStateArray{PlayerArray};

public:
	auto GetPlayerStateArray() { return ToLiveDataView(PlayerStateArray); }

	double GetLatestServerWorldTimeSeconds() const
	{
		return ReplicatedWorldTimeSecondsDouble;
	}

protected:
	virtual void AddPlayerState(APlayerState* PlayerState) override
	{
		Super::AddPlayerState(PlayerState);
		PlayerStateArray.NotifyAdd(PlayerState);
	}

	virtual void RemovePlayerState(APlayerState* PlayerState) override
	{
		Super::RemovePlayerState(PlayerState);
		PlayerStateArray.NotifyRemove(PlayerState);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() != NM_Client)
		{
			AttachServerMachineComponents();
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			AttachPlayerMachineComponents();
		}
	}

	virtual void AttachServerMachineComponents() {}
	virtual void AttachPlayerMachineComponents() {}
};
