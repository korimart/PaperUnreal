// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/WeakCoroutine/CancellableFuture.h"
#include "AreaStateTrackerComponent.generated.h"


UCLASS()
class UAreaStateTrackerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	TCancellableFuture<void> OnlyOneAreaIsSurviving()
	{
		return {};
	}
	
	void SetSpawner(UAreaSpawnerComponent* Spawner)
	{
		check(!HasBeenInitialized());
		AreaSpawner = Spawner;
	}

private:
	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawner;

	UAreaStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(AreaSpawner);
	}
};
