// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryComponent.h"
#include "AreaBoundaryStream.h"
#include "Core/ActorComponent2.h"
#include "Core/WeakCoroutine/WeakCoroutine.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedAreaBoundaryComponent.generated.h"


UCLASS()
class UReplicatedAreaBoundaryComponent : public UActorComponent2, public IAreaBoundaryStream
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TLiveData<FLoopedSegmentArray2D>> GetBoundaryStreamer() override
	{
		return AreaBoundary;
	}

	void SetAreaBoundarySource(UAreaBoundaryComponent* Source)
	{
		BoundarySource = Source;
	}

private:
	UPROPERTY()
	UAreaBoundaryComponent* BoundarySource;

	UPROPERTY(ReplicatedUsing=OnRep_Points)
	TArray<FVector2D> RepPoints;

	TLiveData<FLoopedSegmentArray2D> AreaBoundary;

	UReplicatedAreaBoundaryComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() == NM_Client)
		{
			return;
		}

		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(BoundarySource);
			for (auto Boundaries = BoundarySource->GetBoundaryStreamer().CreateStream();;)
			{
				RepPoints = (co_await AbortOnError(Boundaries)).GetPoints();
			}
		});
	}

	UFUNCTION()
	void OnRep_Points() { AreaBoundary.SetValueAlways(RepPoints); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPoints);
	}
};
