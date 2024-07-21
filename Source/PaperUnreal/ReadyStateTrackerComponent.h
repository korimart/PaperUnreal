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
	TLiveData<int32> ReadyCount{0};

	UPROPERTY()
	TMap<APlayerState*, FDelegateSPHandle> ReadyHandles;

	UReadyStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		GetOuterAGameStateBase2()->GetPlayerStateArray().ObserveAdd(this, [this](APlayerState* PlayerState)
		{
			if (auto ReadyState = PlayerState->FindComponentByClass<UReadyStateComponent>())
			{
				ReadyHandles.FindOrAdd(PlayerState) = ReadyState->GetbReady().Observe([this, PlayerState](bool bReady)
				{
					bReady ? (void)ReadyPlayerStates.Add(PlayerState) : (void)ReadyPlayerStates.Remove(PlayerState);
					ReadyCount.SetValue(ReadyPlayerStates.Num());
				});
			}
		});
		
		GetOuterAGameStateBase2()->GetPlayerStateArray().ObserveRemove(this, [this](APlayerState* PlayerState)
		{
			ReadyHandles.Remove(PlayerState);
			ReadyPlayerStates.Remove(PlayerState);
			ReadyCount.SetValue(ReadyPlayerStates.Num());
		});
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		ReadyHandles.Empty();
	}
};
