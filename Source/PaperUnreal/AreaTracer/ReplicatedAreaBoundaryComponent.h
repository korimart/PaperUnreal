// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryComponent.h"
#include "AreaBoundaryProvider.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedAreaBoundaryComponent.generated.h"


UCLASS()
class UReplicatedAreaBoundaryComponent : public UActorComponent2, public IAreaBoundaryProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<FLoopedSegmentArray2D> GetBoundary() const override { return AreaBoundary; }

	void SetAreaBoundarySource(UAreaBoundaryComponent* Source)
	{
		BoundarySource = Source;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_Points)
	TArray<FVector2D> RepPoints;
	TLiveData<FLoopedSegmentArray2D> AreaBoundary;

	UFUNCTION()
	void OnRep_Points() { AreaBoundary.SetValueNoComparison(RepPoints); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPoints);
	}
	
	UPROPERTY()
	UAreaBoundaryComponent* BoundarySource;

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

		BoundarySource->GetBoundary().Observe(this, [this](const auto& SourceBoundary)
		{
			RepPoints = SourceBoundary.GetPoints();
		});
	}
};
