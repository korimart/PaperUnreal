// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "Core/ActorComponent2.h"
#include "TracerOverlapCheckerComponent.generated.h"


UCLASS()
class UTracerOverlapCheckerComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTracerBumpedInto, UTracerPathComponent*);
	FOnTracerBumpedInto OnTracerBumpedInto;

	void SetTracer(UTracerPathComponent* Tracer)
	{
		check(!HasBeenInitialized());
		OverlapInstigator = Tracer;
	}

	void AddOverlapTarget(UTracerPathComponent* Target)
	{
		check(IsValid(OverlapInstigator));

		if (Target != OverlapInstigator)
		{
			OverlapTargets.Add(Target);
		}
	}

private:
	UPROPERTY()
	UTracerPathComponent* OverlapInstigator;

	UPROPERTY()
	TSet<UTracerPathComponent*> OverlapTargets;

	TOptional<FVector2D> LastCheckPoint;
	FSegment2D MovementThisTick;

	UTracerOverlapCheckerComponent()
	{
		bWantsInitializeComponent = true;
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(OverlapInstigator);
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const FVector2D ThisCheckPoint = FVector2D{GetOwner()->GetActorLocation()};
		
		if (!LastCheckPoint)
		{
			LastCheckPoint = ThisCheckPoint;
			return;
		}
		
		MovementThisTick = FSegment2D{*LastCheckPoint, ThisCheckPoint};
		
		if (MovementThisTick.Length() > UE_KINDA_SMALL_NUMBER)
		{
			OnMaybeOverlappingSelf();
			OnMaybeOverlappingTarget();
			LastCheckPoint = ThisCheckPoint;
		}
	}

	void OnMaybeOverlappingSelf()
	{
		const int32 SegmentCount = OverlapInstigator->GetTracerPath().SegmentCount();

		if (SegmentCount >= 3
			&& OverlapInstigator->GetTracerPath()
			                    .SubArray(0, SegmentCount - 3)
			                    .FindIntersection(MovementThisTick))
		{
			OnTracerBumpedInto.Broadcast(OverlapInstigator);
		}
	}

	void OnMaybeOverlappingTarget()
	{
		for (UTracerPathComponent* Each : OverlapTargets)
		{
			if (IsValid(Each) && Each->GetTracerPath().FindIntersection(MovementThisTick))
			{
				OnTracerBumpedInto.Broadcast(Each);
			}
		}
	}
};
