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

	TValueStream<AAreaActor*> CreateSpawnedAreaStream()
	{
		return CreateMulticastValueStream(RepSpawnedAreas, OnAreaSpawned);
	}

	AAreaActor* SpawnAreaAtRandomEmptyLocation(int32 TeamIndex)
	{
		check(GetNetMode() != NM_Client);

		// TODO 제대로 된 좌표 입력
		AAreaActor* Ret = GetWorld()->SpawnActor<AAreaActor>({1000.f + 500.f * TeamIndex, 1800.f, 50.f}, {});
		Ret->TeamComponent->SetTeamIndex(TeamIndex);
		RepSpawnedAreas.Add(Ret);
		
		return Ret;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedAreas)
	TArray<AAreaActor*> RepSpawnedAreas;
	
	UAreaSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_SpawnedAreas(const TArray<AAreaActor*>& OldAreas)
	{
		const TSet<AAreaActor*> UniqueOldAreas{OldAreas};
		const TSet<AAreaActor*> UniqueNewAreas{RepSpawnedAreas};
		for (AAreaActor* Each : UniqueNewAreas.Difference(UniqueOldAreas))
		{
			if (IsValid(Each))
			{
				OnAreaSpawned.Broadcast(Each);
			}
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepSpawnedAreas);
	}
};
