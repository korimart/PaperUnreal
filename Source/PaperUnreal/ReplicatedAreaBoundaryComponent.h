// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "Core/ActorComponent2.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedAreaBoundaryComponent.generated.h"


UCLASS()
class UReplicatedAreaBoundaryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoundaryChanged, const FLoopedSegmentArray2D&);
	FOnBoundaryChanged OnBoundaryChanged;
	
	void SetAreaBoundarySource(UAreaBoundaryComponent* Source)
	{
		BoundarySource = Source;
	}
	
	FLoopedSegmentArray2D GetBoundary() const
	{
		return RepPoints;
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

		RepPoints = BoundarySource->GetBoundary().GetPoints();
		BoundarySource->OnBoundaryChanged.AddWeakLambda(this, [this]<typename T>(T&& Boundary)
		{
			RepPoints = Forward<T>(Boundary).GetPoints();
		});
	}

	UFUNCTION()
	void OnRep_Points()
	{
		OnBoundaryChanged.Broadcast(GetBoundary());
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPoints);
	}
};
