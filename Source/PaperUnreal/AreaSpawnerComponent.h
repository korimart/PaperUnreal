// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "Core/ActorComponent2.h"
#include "Core/LiveData.h"
#include "AreaSpawnerComponent.generated.h"


UCLASS()
class UAreaSpawnerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAreaSpawned, AAreaActor*);
	FOnAreaSpawned OnAreaSpawned;

	TWeakAwaitable<AAreaActor*> WaitForAreaOfTeam(int32 TeamIndex)
	{
		if (const auto Found = TeamReadySpawnedAreas.FindByPredicate([&](AAreaActor* Each)
		{
			return *Each->TeamComponent->GetTeamIndex().GetValue() == TeamIndex;
		}))
		{
			return *Found;
		}

		TWeakAwaitable<AAreaActor*> Ret;
		TeamReadyAreaAwaitables.FindOrAdd(TeamIndex).Add(Ret.GetHandle());
		return Ret;
	}

	TValueGenerator<AAreaActor*> CreateSpawnedAreaGenerator()
	{
		return CreateMulticastValueGenerator(this, RepSpawnedAreas, OnAreaSpawned);
	}

	AAreaActor* SpawnAreaAtRandomEmptyLocation(int32 TeamIndex)
	{
		check(GetNetMode() != NM_Client);

		AAreaActor* Ret = GetWorld()->SpawnActor<AAreaActor>();

		// TODO 제대로 된 좌표 입력
		Ret->SetActorLocation({1000.f + 500.f * TeamIndex, 1800.f, 50.f});
		Ret->TeamComponent->SetTeamIndex(TeamIndex);
		RepSpawnedAreas.Add(Ret);
		OnAreaTeamReady(Ret, TeamIndex);

		return Ret;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedAreas)
	TArray<AAreaActor*> RepSpawnedAreas;

	UPROPERTY()
	TArray<AAreaActor*> TeamReadySpawnedAreas;

	TMap<int32, TArray<TWeakAwaitableHandle<AAreaActor*>>> TeamReadyAreaAwaitables;

	UAreaSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_SpawnedAreas()
	{
		// TODO 비우는 게 맞을지 새로 추가된 애들만 하는 게 맞는지 생각해보자
		TeamReadySpawnedAreas.Empty();

		for (AAreaActor* Each : RepSpawnedAreas)
		{
			if (!IsValid(Each))
			{
				continue;
			}
			
			RunWeakCoroutine(this, [this, Each](FWeakCoroutineContext& Context) -> FWeakCoroutine
			{
				Context.AddToWeakList(Each);
				UTeamComponent* TeamComponent = co_await WaitForComponent<UTeamComponent>(Each);
				const int32 TeamIndex = co_await TeamComponent->GetTeamIndex().WaitForValue(this);
				OnAreaTeamReady(Each, TeamIndex);
			});
		}
	}

	void OnAreaTeamReady(AAreaActor* Area, int32 TeamIndex)
	{
		TeamReadySpawnedAreas.Add(Area);
		OnAreaSpawned.Broadcast(Area);
		FlushAwaitablesArray(TeamReadyAreaAwaitables.FindOrAdd(TeamIndex), Area);
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepSpawnedAreas);
	}
};
