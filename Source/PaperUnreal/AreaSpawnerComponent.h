// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "Core/ActorComponentEx.h"
#include "Core/LiveData.h"
#include "AreaSpawnerComponent.generated.h"


UCLASS()
class UAreaSpawnerComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	TLiveDataView<AAreaActor*> GetAreaFor(int32 TeamIndex)
	{
		return TeamToAreaMap.FindOrAdd(TeamIndex);
	}

	AAreaActor* SpawnAreaAtRandomEmptyLocation(int32 TeamIndex)
	{
		check(GetNetMode() != NM_Client);
		
		// 일단 팀당 하나의 영역만 스폰 가능하다고 가정
		check(!TeamToAreaMap.Contains(TeamIndex));

		AAreaActor* Ret = GetWorld()->SpawnActor<AAreaActor>();
		
		// TODO 제대로 된 좌표 입력
		Ret->SetActorLocation({1000.f + 500.f * TeamIndex, 1800.f, 50.f});
		Ret->TeamComponent->SetTeamIndex(TeamIndex);
		
		TeamToAreaMap.FindOrAdd(TeamIndex) = Ret;

		// TODO aareaactor의 팀 정보가 변경될 때마다
		// TeamToAreaMap의 캐시된 팀 관계도 업데이트 해야함
		
		return Ret;
	}

private:
	TMap<int32, TLiveData<AAreaActor*>> TeamToAreaMap;
};
