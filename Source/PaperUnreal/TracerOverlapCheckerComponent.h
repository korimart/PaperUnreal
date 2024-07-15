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

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto PathStream = OverlapInstigator->GetTracerPathStreamer().CreateStream();;)
			{
				co_await PathStream.Next();

				OnInstigatorHeadMaybeSelfOverlapping();
				OnInstigatorHeadMaybeOverlapping();

				PrevInstigatorHeadEnd = OverlapInstigator->GetTracerPath().IsValid()
					? OverlapInstigator->GetTracerPath().GetPoint(-1)
					: TOptional<FVector2D>{};
			}
		});
	}

	void OnInstigatorHeadMaybeSelfOverlapping()
	{
		const int32 SegmentCount = OverlapInstigator->GetTracerPath().SegmentCount();
		if (SegmentCount < 2 || !PrevInstigatorHeadEnd)
		{
			return;
		}

		const FSegment2D InstigatorHead = OverlapInstigator->GetTracerPath().GetLastSegment();

		const FSegment2D InstigatorHeadShort
		{
			*PrevInstigatorHeadEnd + InstigatorHead.Direction * UE_KINDA_SMALL_NUMBER,
			InstigatorHead.EndPoint()
		};

		if (OverlapInstigator->GetTracerPath()
		                     .SubArray(0, SegmentCount - 2)
		                     .FindIntersection(InstigatorHeadShort))
		{
			OnTracerBumpedInto.Broadcast(OverlapInstigator);
		}
	}

	void OnInstigatorHeadMaybeOverlapping()
	{
		if (!OverlapInstigator->GetTracerPath().IsValid() || !PrevInstigatorHeadEnd)
		{
			return;
		}

		const FSegment2D InstigatorHead = OverlapInstigator->GetTracerPath().GetLastSegment();

		const FSegment2D PrevInstigatorHead = [&]()
		{
			auto Ret = InstigatorHead;
			Ret.SetEndPoint(*PrevInstigatorHeadEnd);
			return Ret;
		}();

		for (UTracerPathComponent* Each : OverlapTargets)
		{
			if (Each->GetTracerPath().FindIntersection(InstigatorHead)
				&& !Each->GetTracerPath().FindIntersection(PrevInstigatorHead))
			{
				OnTracerBumpedInto.Broadcast(Each);
			}
		}
	}
};
