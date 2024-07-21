// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReadyStateComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/GameStateBase2.h"
#include "Core/Utils.h"
#include "Core/WeakCoroutine/CancellableFuture.h"
#include "GameFramework/PlayerState.h"
#include "ReadyStateTrackerComponent.generated.h"


UCLASS(Within=GameStateBase2)
class UReadyStateTrackerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	TLiveDataView<TLiveData<int32>> GetReadyCount() { return ReadyCount; }

	TCancellableFuture<int32, EValueStreamError> ReadyCountIsAtLeast(int32 Least)
	{
		return GetReadyCount().If([Least](int32 Count) { return Count >= Least; });
	}

private:
	// TODO replace with live data
	UPROPERTY()
	TSet<APlayerState*> ReadyPlayerStates;

	UPROPERTY()
	TMap<APlayerState*, FDelegateSPHandle> PlayerStateToHandleMap;
	
	TLiveData<int32> ReadyCount{0};

	UReadyStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		GetOuterAGameStateBase2()->OnPlayerStateAdded.AddWeakLambda(this, [this](APlayerState* PlayerState)
		{
			if (auto ReadyState = PlayerState->FindComponentByClass<UReadyStateComponent>())
			{
				FDelegateSPHandle Handle = PlayerStateToHandleMap.Emplace(PlayerState, {});
				ReadyState->GetbReady().Observe(Handle.ToShared(), [this, PlayerState](bool bReady)
				{
					bReady ? (void)ReadyPlayerStates.Add(PlayerState) : (void)ReadyPlayerStates.Remove(PlayerState);
					ReadyCount.SetValue(ReadyPlayerStates.Num());
				});
			}
		});
		
		GetOuterAGameStateBase2()->OnPlayerStateRemoved.AddWeakLambda(this, [this](APlayerState* PlayerState)
		{
			PlayerStateToHandleMap.Remove(PlayerState);
			ReadyPlayerStates.Remove(PlayerState);
			ReadyCount.SetValue(ReadyPlayerStates.Num());
		});
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		PlayerStateToHandleMap.Empty();
	}
};
