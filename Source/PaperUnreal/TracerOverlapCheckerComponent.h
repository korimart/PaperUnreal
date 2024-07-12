﻿// Fill out your copyright notice in the Description page of Project Settings.

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
		OverlapInstigator = Tracer;
	}

	void AddOverlapTarget(UTracerPathComponent* Target)
	{
		OverlapTargets.Add(Target);
	}

private:
	UPROPERTY()
	UTracerPathComponent* OverlapInstigator;

	UPROPERTY()
	TSet<UTracerPathComponent*> OverlapTargets;

	TOptional<FVector2D> PrevInstigatorHeadEnd;

	UTracerOverlapCheckerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(OverlapInstigator));

		OverlapInstigator->OnPathEvent.AddWeakLambda(this, [this](auto)
		{
			OnInstigatorHeadMaybeSelfOverlapping();
			OnInstigatorHeadMaybeOverlapping();

			PrevInstigatorHeadEnd = OverlapInstigator->GetPath().IsValid()
				? OverlapInstigator->GetPath().GetPoint(-1)
				: TOptional<FVector2D>{};
		});
	}

	void OnInstigatorHeadMaybeSelfOverlapping()
	{
		const int32 SegmentCount = OverlapInstigator->GetPath().SegmentCount();
		if (SegmentCount < 2 || !PrevInstigatorHeadEnd)
		{
			return;
		}

		const FSegment2D InstigatorHead = OverlapInstigator->GetPath().GetLastSegment();

		const FSegment2D InstigatorHeadShort
		{
			*PrevInstigatorHeadEnd + InstigatorHead.Direction * UE_KINDA_SMALL_NUMBER,
			InstigatorHead.EndPoint()
		};

		if (OverlapInstigator->GetPath()
		                     .SubArray(0, SegmentCount - 2)
		                     .FindIntersection(InstigatorHeadShort))
		{
			OnTracerBumpedInto.Broadcast(OverlapInstigator);
		}
	}

	void OnInstigatorHeadMaybeOverlapping()
	{
		if (!OverlapInstigator->GetPath().IsValid() || !PrevInstigatorHeadEnd)
		{
			return;
		}

		const FSegment2D InstigatorHead = OverlapInstigator->GetPath().GetLastSegment();

		const FSegment2D PrevInstigatorHead = [&]()
		{
			auto Ret = InstigatorHead;
			Ret.SetEndPoint(*PrevInstigatorHeadEnd);
			return Ret;
		}();

		for (UTracerPathComponent* Each : OverlapTargets)
		{
			if (Each->GetPath().FindIntersection(InstigatorHead)
				&& !Each->GetPath().FindIntersection(PrevInstigatorHead))
			{
				OnTracerBumpedInto.Broadcast(Each);
			}
		}
	}
};