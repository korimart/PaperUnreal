// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
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

	TTickingValue<FVector2D> CheckPoint;

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

		CheckPoint.Tick(FVector2D{GetOwner()->GetActorLocation()});
		
		CheckPoint.OverTwoTicks([&](const FVector2D& LastTick, const FVector2D& ThisTick)
		{
			const FSegment2D Movement{LastTick, ThisTick};
			
			if (Movement.Length() > UE_KINDA_SMALL_NUMBER)
			{
				OnMaybeOverlappingSelf(Movement);
				OnMaybeOverlappingTarget(Movement);
			}
		});
	}

	void OnMaybeOverlappingSelf(const FSegment2D& Movement)
	{
		// TODO fix
		const int32 SegmentCount = OverlapInstigator->GetRunningPathAsSegments().SegmentCount();
		if (SegmentCount >= 3
			&& OverlapInstigator->GetRunningPathAsSegments()
			                    .SubArray(0, SegmentCount - 3)
			                    .FindIntersection(Movement))
		{
			OnTracerBumpedInto.Broadcast(OverlapInstigator);
		}
	}

	void OnMaybeOverlappingTarget(const FSegment2D& Movement)
	{
		for (UTracerPathComponent* Each : OverlapTargets)
		{
			if (IsValid(Each) && Each->GetRunningPathAsSegments().FindIntersection(Movement))
			{
				OnTracerBumpedInto.Broadcast(Each);
			}
		}
	}
};
