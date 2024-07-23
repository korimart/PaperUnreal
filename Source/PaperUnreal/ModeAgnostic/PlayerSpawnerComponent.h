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

	template <typename FuncType>
	APawn* SpawnAtLocation(UClass* Class, const FVector& Location, const FuncType& Initializer)
	{
		check(GetNetMode() != NM_Client);
		
		APawn* Spawned = CastChecked<APawn>(GetWorld()->SpawnActor(Class, &Location));
		Initializer(Spawned);
		
		const TArray<APawn*> Prev = RepPlayers;
		RepPlayers.Add(Spawned);
		OnRep_SpawnedPlayers(Prev);

		return Spawned;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedPlayers)
	TArray<APawn*> RepPlayers;
	
	TBackedLiveData<
		TArray<APawn*>,
		ERepHandlingPolicy::CompareForAddOrRemove
	> Players{RepPlayers};

	UPlayerSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_SpawnedPlayers(const TArray<APawn*>& Prev) { Players.OnRep(Prev); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPlayers);
	}
};
