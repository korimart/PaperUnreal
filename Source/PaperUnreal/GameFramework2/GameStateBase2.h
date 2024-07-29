// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "GameStateBase2.generated.h"

/**
 * 
 */
UCLASS()
class AGameStateBase2 : public AGameStateBase
{
	GENERATED_BODY()

	mutable TLiveData<TArray<TObjectPtr<APlayerState>>&> PlayerStateArray{PlayerArray};

public:
	TLiveDataView<TArray<TObjectPtr<APlayerState>>&> GetPlayerStateArray() const { return PlayerStateArray; }

	double GetLatestServerWorldTimeSeconds() const
	{
		return ReplicatedWorldTimeSecondsDouble;
	}

protected:
	virtual void AddPlayerState(APlayerState* PlayerState) override
	{
		const auto Old = PlayerArray;
		Super::AddPlayerState(PlayerState);
		PlayerStateArray.NotifyDiff(Old);
	}

	virtual void RemovePlayerState(APlayerState* PlayerState) override
	{
		const auto Old = PlayerArray;
		Super::RemovePlayerState(PlayerState);
		PlayerStateArray.NotifyDiff(Old);
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
