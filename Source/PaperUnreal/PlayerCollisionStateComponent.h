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
	TLiveDataView<bool> FindOrAddCollisionWith(UAreaMeshComponent* AreaMeshComponent)
	{
		return AreaToCollisionStatusMap.FindOrAdd(AreaMeshComponent);
	}

	FVector GetPlayerLocation() const
	{
		return GetOwner()->GetActorLocation();
	}

private:
	TMap<TWeakObjectPtr<UAreaMeshComponent>, TLiveData<bool>> AreaToCollisionStatusMap;

	UPlayerCollisionStateComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const FVector CurrentPosition{GetOwner()->GetActorLocation()};

		for (auto& [EachArea, EachStatus] : AreaToCollisionStatusMap)
		{
			// TODO remove if not valid
			if (EachArea.IsValid())
			{
				// TODO remove if not bound
				EachStatus.SetValue(EachArea->IsInside(CurrentPosition));
			}
		}
	}
};
