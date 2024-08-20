// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryComponent.h"
#include "TracerPathProvider.h"
#include "TracerPathComponent.generated.h"


UCLASS()
class UTracerPathComponent : public UActorComponent2, public ITracerPathProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TOptional<FVector2D>> GetRunningPathHead() const override { return PathHead; }
	virtual TLiveDataView<TArray<FVector2D>> GetRunningPathTail() const override { return PathTail; }
	
	TLiveDataView<TOptional<FSegmentArray2D>> GetLastCompletePath() const { return LastCompletePath; }
	
	const FSegmentArray2D& GetRunningPath() const { return Path; }

	// TODO remove dependency
	void SetNoPathArea(UAreaBoundaryComponent* Area) { NoPathArea = Area; }

	void ClearPath() { EmptyPoints(); }

private:
	UPROPERTY()
	UAreaBoundaryComponent* NoPathArea;

	mutable TLiveData<TOptional<FVector2D>> PathHead;
	mutable TLiveData<TArray<FVector2D>> PathTail;
	mutable TLiveData<TOptional<FSegmentArray2D>> LastCompletePath;
	FSegmentArray2D Path;
	
	FTickingSwitch Switch;

	UTracerPathComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		EmptyPoints();
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bAlwaysGenerate = !IsValid(NoPathArea);
		const bool bAreaHasNonZeroArea = IsValid(NoPathArea) && NoPathArea->IsValid();
		const bool bOwnerIsOutsideArea = IsValid(NoPathArea) && !NoPathArea->IsInside(GetOwner()->GetActorLocation());
		const bool bGeneratePath = bAlwaysGenerate || (bAreaHasNonZeroArea && bOwnerIsOutsideArea);

		Switch.Tick(bGeneratePath);

		Switch.IfTrueThisFrame([&]()
		{
			Generate();
		});

		Switch.IfSwitchedOnThisFrame([&]()
		{
			AttachHeadToArea();
		});

		Switch.IfSwitchedOffThisFrame([&]()
		{
			AttachHeadToArea();
			EmptyPoints();
		});
	}

	void AttachHeadToArea()
	{
		if (IsValid(NoPathArea) && NoPathArea->IsValid())
		{
			const FVector2D Attached = NoPathArea->FindClosestPointOnBoundary2D(Path.GetPoint(-1)).GetPoint();
			Path.SetPoint(-1, Attached);
			PathHead.SetValue(Attached);
		}
	}

	void AddPoint(const FVector2D& Location)
	{
		PathHead.SetValue(TOptional<FVector2D>{});
		Path.AddPoint(Location);
		PathTail.Add(Location);
		PathHead.SetValue(Location);
	}

	void EmptyPoints()
	{
		PathHead.SetValue(TOptional<FVector2D>{});
		PathTail.Empty();
		LastCompletePath.SetValueNoComparison(MoveTemp(Path));
	}

	void Generate()
	{
		const FVector2D ActorLocation2D{GetOwner()->GetActorLocation()};

		if (Path.PointCount() == 0)
		{
			AddPoint(ActorLocation2D);
			return;
		}

		if (Path.GetPoint(-1).Equals(ActorLocation2D))
		{
			return;
		}

		if (Path.PointCount() < 3)
		{
			AddPoint(ActorLocation2D);
			return;
		}

		const float Curvature = [&]()
		{
			const FVector2D& Position0 = Path.GetPoint(-2);
			const FVector2D& Position1 = Path.GetPoint(-1);
			const FVector2D& Position2 = ActorLocation2D;

			const float ASideLength = (Position1 - Position2).Length();
			const float BSideLength = (Position0 - Position2).Length();
			const float CSideLength = (Position0 - Position1).Length();

			const float TriangleArea = 0.5f * FMath::Abs(
				Position0.X * (Position1.Y - Position2.Y)
				+ Position1.X * (Position2.Y - Position0.Y)
				+ Position2.X * (Position0.Y - Position1.Y));

			return 4.f * TriangleArea / ASideLength / BSideLength / CSideLength;
		}();

		const float CurrentDeviation = [&]()
		{
			const FVector2D DeviatingVector = ActorLocation2D - Path.GetPoint(-2);
			const FVector2D StraightVector = Path.GetSegmentDirection(-2);
			const FVector2D Proj = StraightVector * DeviatingVector.Dot(StraightVector);
			return (DeviatingVector - Proj).Length();
		}();

		if (Curvature > 0.005f || CurrentDeviation > 1.f)
		{
			AddPoint(ActorLocation2D);
		}
		else
		{
			Path.SetPoint(-1, ActorLocation2D);
			PathHead.SetValue(ActorLocation2D);
		}
	}
};
