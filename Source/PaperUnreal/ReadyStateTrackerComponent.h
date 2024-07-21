// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReadyStateComponent.h"
#include "StateTrackerComponent.h"
#include "Algo/Accumulate.h"
#include "Core/GameStateBase2.h"
#include "Core/Utils.h"
#include "Core/WeakCoroutine/CancellableFuture.h"
#include "GameFramework/PlayerState.h"
#include "ReadyStateTrackerComponent.generated.h"


namespace ReadyStateTracker
{
	inline FName ReadyHandle{ TEXT("ReadyStateTracker_ReadyHandle")};
}


UCLASS(Within=GameStateBase2)
class UReadyStateTrackerComponent : public UStateTrackerComponent
{
	GENERATED_BODY()

public:
	TCancellableFuture<void> ReadyCountIsAtLeast(int32 Least)
	{
		return MakeConditionedPromise([this, Least]()
		{
			return Algo::TransformAccumulate(
				GetComponents<UReadyStateComponent>(GetOuterAGameStateBase2()->GetPlayerStateArray().Get()),
				[](auto Each) { return Each->GetbReady().Get() ? 1 : 0; },
				0) >= Least;
		});
	}

private:
	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		GetOuterAGameStateBase2()->GetPlayerStateArray().ObserveAdd(this, [this](APlayerState* PlayerState)
		{
			if (auto ReadyState = PlayerState->FindComponentByClass<UReadyStateComponent>())
			{
				UniqueHandle(PlayerState, ReadyStateTracker::ReadyHandle) = ReadyState->GetbReady().Observe([this](bool)
				{
					OnSomeConditionMaybeSatisfied();
				});
			}
		});

		GetOuterAGameStateBase2()->GetPlayerStateArray().ObserveRemove(this, [this](APlayerState* PlayerState)
		{
			RemoveHandles(PlayerState);
			OnSomeConditionMaybeSatisfied();
		});
	}
};
