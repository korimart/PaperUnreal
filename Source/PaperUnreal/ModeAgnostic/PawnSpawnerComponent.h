﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PawnSpawnerComponent.generated.h"


UCLASS()
class UPawnSpawnerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	TLiveDataView<TArray<APawn*>&> GetSpawnedPawns() { return Pawns; }
	
	APawn* SpawnAtLocation(UClass* Class, const FVector& Location)
	{
		return SpawnAtLocation(Class, Location, [](auto){});
	}

	APawn* SpawnAtLocation(UClass* Class, const FVector& Location, const auto& Initializer)
	{
		check(GetNetMode() != NM_Client);
		
		APawn* Spawned = CastChecked<APawn>(GetWorld()->SpawnActor(Class, &Location));
		
		Initializer(Spawned);
		
		auto NewComponent = NewObject<UActorComponent2>(Spawned);
		NewComponent->RegisterComponent();
		NewComponent->OnEndPlay.AddWeakLambda(this, [this, Spawned]() { Pawns.Remove(Spawned); });
		
		Pawns.Add(Spawned);
		
		return Spawned;
	}

	void DestroyPawnsOnEndPlay()
	{
		bDestroyPawnsOnEndPlay = true;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedPawns)
	TArray<APawn*> RepPawns;
	TLiveData<TArray<APawn*>&> Pawns{RepPawns};
	
	UFUNCTION()
	void OnRep_SpawnedPawns(const TArray<APawn*>& Prev) { Pawns.NotifyDiff(Prev); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPawns);
	}

	bool bDestroyPawnsOnEndPlay = false;
	
	UPawnSpawnerComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		if (bDestroyPawnsOnEndPlay)
		{
			TArray<APawn*> ToDestroy = MoveTemp(Pawns.Get());
			for (APawn* Each : ToDestroy)
			{
				if (IsValid(Each))
				{
					Each->Destroy();
				}
			}
		}
	}
};
