// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/LiveData.h"
#include "PlayerCollisionStateComponent.generated.h"


UCLASS()
class UPlayerCollisionStateComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void AddCollisionTarget(UAreaMeshComponent* AreaMeshComponent)
	{
		Areas.Add(AreaMeshComponent);
	}

	TLiveDataView<bool> GetCollisionWith(UAreaMeshComponent* AreaMeshComponent) const
	{
		return AreaToCollisionStatusMap.FindOrAdd(AreaMeshComponent);
	}

private:
	UPROPERTY()
	TSet<UAreaMeshComponent*> Areas;

	mutable TMap<TWeakObjectPtr<UAreaMeshComponent>, TLiveData<bool>> AreaToCollisionStatusMap;
	
	UPlayerCollisionStateComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const FVector CurrentPosition{ GetOwner()->GetActorLocation() };

		for (UAreaMeshComponent* Each : Areas)
		{
			AreaToCollisionStatusMap.FindOrAdd(Each).SetValue(Each->IsInside(CurrentPosition));
		}
	}
};
