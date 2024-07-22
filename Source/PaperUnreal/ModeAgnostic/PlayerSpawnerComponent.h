// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PlayerSpawnerComponent.generated.h"


UCLASS()
class UPlayerSpawnerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	auto GetSpawnedPlayers() { return ToLiveDataView(Players); }

	AActor* SpawnAtLocation(UClass* Class, const FVector& Location)
	{
		check(GetNetMode() != NM_Client);
		
		AActor* Spawned = GetWorld()->SpawnActor(Class, &Location);
		const TArray<AActor*> Prev = RepPlayers;
		RepPlayers.Add(Spawned);
		OnRep_SpawnedPlayers(Prev);

		return Spawned;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedPlayers)
	TArray<AActor*> RepPlayers;
	
	TBackedLiveData<
		TArray<AActor*>,
		ERepHandlingPolicy::CompareForAddOrRemove
	> Players{RepPlayers};

	UPlayerSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_SpawnedPlayers(const TArray<AActor*>& Prev) { Players.OnRep(Prev); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPlayers);
	}
};
