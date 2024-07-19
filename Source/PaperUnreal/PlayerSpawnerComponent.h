// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/WeakCoroutine/ValueStream.h"
#include "Net/UnrealNetwork.h"
#include "PlayerSpawnerComponent.generated.h"


UCLASS()
class UPlayerSpawnerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	const TValueStreamer<AActor*>& GetSpawnedPlayerStreamer() const { return SpawnedPlayerStreamer; }

	AActor* SpawnAtLocation(UClass* Class, const FVector& Location)
	{
		check(GetNetMode() != NM_Client);
		
		AActor* Spawned = GetWorld()->SpawnActor(Class, &Location);
		RepPlayers.Add(Spawned);
		OnRep_SpawnedPlayers();

		return Spawned;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedPlayers)
	TArray<AActor*> RepPlayers;

	TValueStreamer<AActor*> SpawnedPlayerStreamer;

	UPlayerSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_SpawnedPlayers() { SpawnedPlayerStreamer.ReceiveValuesIfNotInHistory(RepPlayers); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPlayers);
	}
};
