// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryComponent.h"
#include "TracerPathGenerator.h"
#include "TracerPathComponent.generated.h"


UCLASS()
class UTracerPathComponent : public UActorComponent2, public ITracerPathStream
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TOptional<FTracerPathPoint2>> GetRunningPathHead() const override
	{
		return PathHead;
	}

	virtual TLiveDataView<TArray<FTracerPathPoint2>> GetRunningPathTail() const override
	{
		return PathTail;
	}

	const FSegmentArray2D& GetRunningPathAsSegments() const
	{
		return Path;
	}

	// TODO remove dependency
	void SetNoPathArea(UAreaBoundaryComponent* Area)
	{
		NoPathArea = Area;
	}

private:
	UPROPERTY()
	UAreaBoundaryComponent* NoPathArea;

	mutable TLiveData<TOptional<FTracerPathPoint2>> PathHead;
	mutable TLiveData<TArray<FTracerPathPoint2>> PathTail;
	FSegmentArray2D Path;
	FTickingSwitch Switch;

	UTracerPathComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(NoPathArea);
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		EmptyPoints();
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bAreaHasNonZeroArea = NoPathArea->IsValid();
		const bool bOwnerIsOutsideArea = !NoPathArea->IsInside(GetOwner()->GetActorLocation());
		const bool bGeneratePath = bAreaHasNonZeroArea && bOwnerIsOutsideArea;
		
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
		if (NoPathArea->IsValid())
		{
			const FVector2D Attached = NoPathArea->FindClosestPointOnBoundary2D(Path.GetLastPoint()).GetPoint();
			Path.SetPoint(-1, Attached);
			PathHead.SetValue(FTracerPathPoint2{Attached, PathHead.GetValid().PathDirection});
		}
	}

	void AddPoint(const FVector2D& Location, const FVector2D& Direction)
	{
		PathHead.SetValue(TOptional<FTracerPathPoint2>{});
		Path.AddPoint(Location);
		PathTail.Add(FTracerPathPoint2{Location, Direction});
		PathHead.SetValue(FTracerPathPoint2{Location, Direction});
	}

	void EmptyPoints()
	{
		Path.Empty();
		PathHead.SetValue(TOptional<FTracerPathPoint2>{});
		PathTail.Empty();
	}

	void Generate()
	{
		const FVector2D ActorLocation2D{GetOwner()->GetActorLocation()};
		const FVector2D ActorDirection2D{GetOwner()->GetActorForwardVector()};

		if (Path.PointCount() == 0)
		{
			AddPoint(ActorLocation2D, ActorDirection2D);
			return;
		}

		if (Path.GetLastPoint().Equals(ActorLocation2D))
		{
			return;
		}

		if (Path.PointCount() < 3)
		{
			AddPoint(ActorLocation2D, ActorDirection2D);
			return;
		}

		const float Curvature = [&]()
		{
			const FVector2D& Position0 = Path.GetLastPoint(1);
			const FVector2D& Position1 = Path.GetLastPoint();
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
			const FVector2D DeviatingVector = ActorLocation2D - Path.GetLastPoint(1);
			const FVector2D StraightVector = Path.GetLastSegmentDirection(1);
			const FVector2D Proj = StraightVector * DeviatingVector.Dot(StraightVector);
			return (DeviatingVector - Proj).Length();
		}();

		if (Curvature > 0.005f || CurrentDeviation > 1.f)
		{
			AddPoint(ActorLocation2D, ActorDirection2D);
		}
		else
		{
			Path.SetPoint(-1, ActorLocation2D);
			PathHead.SetValue(FTracerPathPoint2{ActorLocation2D, ActorDirection2D});
		}
	}
};
