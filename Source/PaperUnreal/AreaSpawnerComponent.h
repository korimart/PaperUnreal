// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "Core/ActorComponentEx.h"
#include "Core/UECoroutine.h"
#include "AreaSpawnerComponent.generated.h"


UCLASS()
class UAreaSpawnerComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAreaSpawned, AAreaActor*);
	FOnAreaSpawned& GetAreaSpawnedDelegateFor(int32 TeamIndex) const
	{
		return TeamToAreaSpawnedDelegateMap.FindOrAdd(TeamIndex);
	}
	
	AAreaActor* SpawnAreaAtRandomEmptyLocation(int32 TeamIndex)
	{
		// 일단 팀당 하나의 영역만 스폰 가능하다고 가정
		check(!TeamToAreaMap.Contains(TeamIndex));
		
		AAreaActor* Ret = GetWorld()->SpawnActor<AAreaActor>();
		Ret->SetTeamIndex(TeamIndex);

		// TODO
		Ret->SetActorLocation({1000.f + 500.f * TeamIndex, 1800.f, 50.f});
		
		TeamToAreaMap.FindOrAdd(TeamIndex) = Ret;

		GetAreaSpawnedDelegateFor(TeamIndex).Broadcast(Ret);
		
		return Ret;
	}
	
	TWeakAwaitable<AAreaActor*> WaitForAreaBelongingTo(int32 TeamIndex)
	{
		if (AAreaActor* Found = TeamToAreaMap.FindRef(TeamIndex))
		{
			return Found;
		}

		return WaitForBroadcast(this, GetAreaSpawnedDelegateFor(TeamIndex));
	}
	
private:
	UPROPERTY()
	TMap<int32, AAreaActor*> TeamToAreaMap;
	
	mutable TMap<int32, FOnAreaSpawned> TeamToAreaSpawnedDelegateMap;
};