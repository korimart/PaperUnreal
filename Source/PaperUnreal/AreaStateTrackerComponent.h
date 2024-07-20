// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/WeakCoroutine/CancellableFuture.h"
#include "AreaStateTrackerComponent.generated.h"


// TODO 이 클래스 구현 ReadyStateTracker랑 완전히 똑같다
UCLASS()
class UAreaStateTrackerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetSpawner(UAreaSpawnerComponent* Spawner)
	{
		check(!HasBeenInitialized());
		AreaSpawner = Spawner;
	}

	TCancellableFuture<int32, EValueStreamError> OnlyOneAreaIsSurviving()
	{
		return TLiveDataView{LiveAreaCount}.If(1);
	}

private:
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawner;

	UPROPERTY()
	TSet<AAreaActor*> LiveAreas;

	UPROPERTY()
	TMap<AAreaActor*, FDelegateSPHandle> AreaToHandleMap;

	TLiveData<int32> LiveAreaCount{0};

	UAreaStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(AreaSpawner);

		AreaSpawner->GetSpawnedAreaStreamer().Observe(this, [this](AAreaActor* SpawnedArea)
		{
			FDelegateSPHandle Handle = AreaToHandleMap.Emplace(SpawnedArea, {});
			SpawnedArea->LifeComponent->GetbAlive().Observe(Handle.ToShared(), [this, SpawnedArea](bool bAlive)
			{
				bAlive ? (void)LiveAreas.Add(SpawnedArea) : (void)LiveAreas.Remove(SpawnedArea);
				LiveAreaCount.SetValue(LiveAreas.Num());
			});
		});
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		AreaToHandleMap.Empty();
	}
};
