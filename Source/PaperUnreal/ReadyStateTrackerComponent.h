// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReadyStateComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/GameStateBase2.h"
#include "Core/WeakCoroutine/CancellableFuture.h"
#include "GameFramework/PlayerState.h"
#include "ReadyStateTrackerComponent.generated.h"


UCLASS(Within=GameStateBase2)
class UReadyStateTrackerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	TLiveDataView<int32> GetReadyCount() { return ReadyCount; }

	TCancellableFuture<int32> WaitUntilCountIsAtLeast(int32 Least)
	{
		return GetReadyCount().WaitForValue([Least](int32 Count) { return Count >= Least; });
	}

private:
	TLiveData<int32> ReadyCount{0};

	UPROPERTY()
	TSet<APlayerState*> ReadyPlayerStates;

	UReadyStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		// PlayerState가 한 번 추가되면 같은 PlayerState가 다시 추가되지 않는다고 가정함
		// (즉 플레이어가 한 번 나갔다 오면 APlayerState 액터가 재사용 되는 것이 아닌 새 것이 만들어진다고 가정함)
		// 가정이 깨질 경우 한 액터에 여러번 콜백을 바인딩 할 수 있음
		GetOuterAGameStateBase2()->OnPlayerStateAdded.AddWeakLambda(this, [this](APlayerState* NewPlayerState)
		{
			if (UReadyStateComponent* ReadyState = NewPlayerState->FindComponentByClass<UReadyStateComponent>())
			{
				ReadyState->GetbReady().Observe(this, [this, NewPlayerState](bool bReady)
				{
					if (bReady)
					{
						ReadyPlayerStates.Add(NewPlayerState);
					}
					else
					{
						ReadyPlayerStates.Remove(NewPlayerState);
					}

					ReadyCount.SetValue(ReadyPlayerStates.Num());
				});
			}
		});
	}
};
