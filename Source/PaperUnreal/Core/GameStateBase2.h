// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameStateBase2.generated.h"

/**
 * 
 */
UCLASS()
class AGameStateBase2 : public AGameStateBase
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FPlayerStateEvent, APlayerState*);
	FPlayerStateEvent OnPlayerStateAdded;
	FPlayerStateEvent OnPlayerStateRemoved;

	double GetLatestServerWorldTimeSeconds() const
	{
		return ReplicatedWorldTimeSecondsDouble;
	}

protected:
	virtual void AddPlayerState(APlayerState* PlayerState) override
	{
		Super::AddPlayerState(PlayerState);
		OnPlayerStateAdded.Broadcast(PlayerState);
	}

	virtual void RemovePlayerState(APlayerState* PlayerState) override
	{
		Super::RemovePlayerState(PlayerState);
		OnPlayerStateRemoved.Broadcast(PlayerState);
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
