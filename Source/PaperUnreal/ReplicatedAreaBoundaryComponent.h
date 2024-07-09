// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryStream.h"
#include "AreaMeshComponent.h"
#include "Core/ActorComponent2.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedAreaBoundaryComponent.generated.h"


UCLASS()
class UReplicatedAreaBoundaryComponent : public UActorComponent2, public IAreaBoundaryStream
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoundaryChanged, const FLoopedSegmentArray2D&);
	FOnBoundaryChanged OnBoundaryChanged;
	
	virtual TValueStream<FLoopedSegmentArray2D> CreateBoundaryStream() override
	{
		TArray<FLoopedSegmentArray2D> Init{{RepPoints}};
		if (Init[0].IsValid()) { return CreateMulticastValueStream(Init, OnBoundaryChanged); }
		return CreateMulticastValueStream(TArray<FLoopedSegmentArray2D>{}, OnBoundaryChanged);
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

		check(AllValid(BoundarySource));

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto Boundaries = BoundarySource->CreateBoundaryStream();;)
			{
				RepPoints = (co_await Boundaries.Next()).GetPoints();
			}
		});
	}

	UFUNCTION()
	void OnRep_Points()
	{
		OnBoundaryChanged.Broadcast(RepPoints);
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPoints);
	}
};
