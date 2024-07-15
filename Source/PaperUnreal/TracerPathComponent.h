// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryComponent.h"
#include "AreaMeshComponent.h"
#include "TracerPathGenerator.h"
#include "TracerPathComponent.generated.h"


UCLASS()
class UTracerPathComponent : public UActorComponent2, public ITracerPathStream
{
	GENERATED_BODY()

public:
	virtual const TValueStreamer<FTracerPathEvent>& GetTracerPathStreamer() const override
	{
		return TracerPathStreamer;
	}

	const FSegmentArray2D& GetTracerPath() const
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

	bool bGeneratedThisFrame = false;
	FSegmentArray2D Path;
	TValueStreamer<FTracerPathEvent> TracerPathStreamer;

	UTracerPathComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(NoPathArea));
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		TracerPathStreamer.EndStreams();
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bGeneratedLastFrame = bGeneratedThisFrame;
		const bool bWillGenerateThisFrame = !NoPathArea->IsInside(GetOwner()->GetActorLocation());
		bGeneratedThisFrame = bWillGenerateThisFrame;
		
		if (bWillGenerateThisFrame)
		{
			Generate();
		}

		if (bGeneratedLastFrame && !bGeneratedThisFrame)
		{
			Path.SetPoint(-1, NoPathArea->FindClosestPointOnBoundary2D(Path.GetLastPoint()).GetPoint());
			TracerPathStreamer.ReceiveValue(CreateEvent(EStreamEvent::LastModified));
			TracerPathStreamer.EndStreams();
			Path.Empty();
		}
	}

	void Generate()
	{
		const FVector2D ActorLocation2D{GetOwner()->GetActorLocation()};

		if (Path.PointCount() == 0)
		{
			Path.AddPoint(ActorLocation2D);
			Path.SetPoint(-1, NoPathArea->FindClosestPointOnBoundary2D(Path.GetLastPoint()).GetPoint());
			TracerPathStreamer.ReceiveValue(CreateEvent(EStreamEvent::Appended));
			return;
		}

		if (Path.GetLastPoint().Equals(ActorLocation2D))
		{
			return;
		}

		if (Path.PointCount() < 3)
		{
			Path.AddPoint(ActorLocation2D);
			TracerPathStreamer.ReceiveValue(CreateEvent(EStreamEvent::Appended));
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

		if (Curvature > 0.005f || CurrentDeviation > 10.f)
		{
			Path.AddPoint(ActorLocation2D);
			TracerPathStreamer.ReceiveValue(CreateEvent(EStreamEvent::Appended));
		}
		else
		{
			Path.SetPoint(-1, ActorLocation2D);
			TracerPathStreamer.ReceiveValue(CreateEvent(EStreamEvent::LastModified));
		}
	}

	FTracerPathEvent CreateEvent(EStreamEvent Event) const
	{
		return {
			.Event = Event,
			.Affected = {
				{
					.Point = Path.GetLastPoint(),
					.PathDirection = FVector2D{GetOwner()->GetActorForwardVector()}
				}
			}
		};
	}
};
