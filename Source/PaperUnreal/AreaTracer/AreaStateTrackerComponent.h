// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/StateTrackerComponent.h"
#include "AreaStateTrackerComponent.generated.h"


namespace AreaStateTracker
{
	inline FName AliveHandle{TEXT("AreaStateTracer_AliveHandle")};
}


UCLASS()
class UAreaStateTrackerComponent : public UStateTrackerComponent
{
	GENERATED_BODY()

public:
	void SetSpawner(UAreaSpawnerComponent* Spawner)
	{
		check(!HasBeenInitialized());
		AreaSpawner = Spawner;
	}

	TCancellableFuture<void> ZeroOrOneAreaIsSurviving()
	{
		return MakeConditionedPromise([this]()
		{
			return Algo::TransformAccumulate(
				GetComponents<ULifeComponent>(AreaSpawner->GetSpawnedAreas().Get()),
				[](auto Each) { return Each->GetbAlive().Get() ? 1 : 0; },
				0) <= 1;
		});
	}

private:
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawner;

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(AreaSpawner);

		AreaSpawner->GetSpawnedAreas().ObserveAdd(this, [this](AAreaActor* SpawnedArea)
		{
			if (!IsValid(SpawnedArea))
			{
				return;
			}
			
			UniqueHandle(SpawnedArea, AreaStateTracker::AliveHandle)
				= SpawnedArea->LifeComponent->GetbAlive().Observe([this, SpawnedArea](bool)
				{
					OnSomeConditionMaybeSatisfied();
				});
		});

		AreaSpawner->GetSpawnedAreas().ObserveRemove(this, [this](AAreaActor* SpawnedArea)
		{
			RemoveHandles(SpawnedArea);
			OnSomeConditionMaybeSatisfied();
		});
	}
};
