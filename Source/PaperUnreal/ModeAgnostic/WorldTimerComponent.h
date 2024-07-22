// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "WorldTimerComponent.generated.h"


UCLASS(Within=GameStateBase2)
class UWorldTimerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	TCancellableFuture<void> At(float WorldSeconds)
	{
		auto [Promise, Future] = MakePromise<void>();
		PendingTimers.Emplace(WorldSeconds, MoveTemp(Promise));
		return MoveTemp(Future);
	}

private:
	struct FPendingTimer
	{
		float WorldTimeToFireAt;
		TCancellablePromise<void> Promise;
	};

	TArray<FPendingTimer> PendingTimers;

	UWorldTimerComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const int32 UpperBound = Algo::UpperBoundBy(
			PendingTimers,
			GetOuterAGameStateBase2()->GetLatestServerWorldTimeSeconds(),
			&FPendingTimer::WorldTimeToFireAt);

		TArray<FPendingTimer> ToCall;

		for (int32 i = 0; i < UpperBound; i++)
		{
			ToCall.Emplace(PopFront(PendingTimers));
		}

		for (FPendingTimer& Each : ToCall)
		{
			Each.Promise.SetValue();
		}
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);
		PendingTimers.Empty();
	}
};
