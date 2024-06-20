﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentArray.h"
#include "Algo/AllOf.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/PlanarPolygonMeshGenerator.h"
#include "AreaMeshComponent.generated.h"


/**
 * counter-clockwise
 */
class FPolygonBoundary2D
{
public:
	struct FSegment
	{
		int32 StartPointIndex;
		int32 EndPointIndex;
	};

	struct FIntersection
	{
		FSegment HitSegment;
		FVector2D PointOfIntersection;
	};

	FPolygonBoundary2D& operator=(FLoopedSegmentArray2D&& Other)
	{
		Segments = MoveTemp(Other);
		return *this;
	}

	bool IsInside(const FVector2D& Point) const
	{
		if (!Segments.IsValid())
		{
			return false;
		}

		const auto IsSegmentToTheLeftOfPoint = [&](const std::tuple<FVector2D, FVector2D>& SegmentEnds)
		{
			const FVector2D Line = std::get<1>(SegmentEnds) - std::get<0>(SegmentEnds);
			const FVector2D ToPoint = Point - std::get<0>(SegmentEnds);
			const double CrossProduct = FVector2D::CrossProduct(Line, ToPoint);
			return CrossProduct < 0.;
		};

		return Algo::AllOf(Segments, IsSegmentToTheLeftOfPoint);
	}

	const FLoopedSegmentArray2D& GetBoundarySegmentArray() const
	{
		return Segments;
	}

	FIntersection FindClosestPointTo(const FVector2D& Point) const
	{
		FIntersection Ret{};
		float Distance = TNumericLimits<float>::Max();
		for (auto [It, End] = std::tuple{Segments.begin(), Segments.end()}; It != End; ++It)
		{
			const auto [EachSegmentStart, EachSegmentEnd] = *It;

			const FVector2D SegmentVector = EachSegmentEnd - EachSegmentStart;
			const FVector2D ToPointVector = Point - EachSegmentStart;
			const float ProjCoefficient = FMath::Clamp(SegmentVector.Dot(ToPointVector) / SegmentVector.SizeSquared(), 0.f, 1.f);
			const FVector2D ProjPointOnSegment = EachSegmentStart + ProjCoefficient * SegmentVector;
			const float DistToPoint = (Point - ProjPointOnSegment).Length();

			if (DistToPoint < Distance)
			{
				Distance = DistToPoint;
				Ret.HitSegment.StartPointIndex = It.GetStartPointIndex();
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.PointOfIntersection = ProjPointOnSegment;
			}
		}
		return Ret;
	}

	TOptional<FIntersection> FindIntersectionWithSegment(const FVector2D& SegmentStart, const FVector2D& SegmentEnd) const
	{
		if (!Segments.IsValid())
		{
			return {};
		}

		for (auto [It, End] = std::tuple{Segments.begin(), Segments.end()}; It != End; ++It)
		{
			const auto [EachSegmentStart, EachSegmentEnd] = *It;

			FVector Intersection;
			if (FMath::SegmentIntersection2D(
				FVector{SegmentStart, 0.f},
				FVector{SegmentEnd, 0.f},
				FVector{EachSegmentStart, 0.f},
				FVector{EachSegmentEnd, 0.f},
				Intersection))
			{
				FIntersection Ret;
				Ret.HitSegment.StartPointIndex = It.GetStartPointIndex();
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.PointOfIntersection = FVector2D{Intersection};
				return Ret;
			}
		}

		return {};
	}

	void Replace(int32 FirstIndex, int32 LastIndex, const TArray<FVector2D>& NewPoints)
	{
		Segments.ReplacePoints(FirstIndex, LastIndex, NewPoints);
	}

private:
	FLoopedSegmentArray2D Segments;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAreaMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	using FIntersection = FPolygonBoundary2D::FIntersection;

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary.IsInside(FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(Point)});
	}

	FVector2D World2DToLocal2D(const FVector2D& World2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(FVector{World2D, 0.f})};
	}

	FIntersection FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		FIntersection Ret = AreaBoundary.FindClosestPointTo(World2DToLocal2D(Point));
		const FVector WorldPoint = GetOwner()->GetActorTransform().TransformPosition(FVector{Ret.PointOfIntersection, 0.f});
		Ret.PointOfIntersection = FVector2D{WorldPoint};
		return Ret;
	}

	void ExpandByEnclosingPath(FSegmentArray2D Path)
	{
		const bool bPathIsClockwise = Path.CalculateNetAngleDelta() > 0.f;
		if (bPathIsClockwise)
		{
			Path.ReverseVertexOrder();
		}

		Path.ApplyToEachPoint([this](FVector2D& Each) { Each = World2DToLocal2D(Each); });

		const FIntersection BoundarySrcSegment = AreaBoundary.FindClosestPointTo(Path.GetPoints()[0]);
		const FIntersection BoundaryDestSegment = AreaBoundary.FindClosestPointTo(Path.GetLastPoint());

		FLoopedSegmentArray2D Option0 = AreaBoundary.GetBoundarySegmentArray();
		Option0.ReplacePoints(
			BoundarySrcSegment.HitSegment.EndPointIndex,
			BoundaryDestSegment.HitSegment.StartPointIndex,
			Path.GetPoints());

		Path.ReverseVertexOrder();
		FLoopedSegmentArray2D Option1 = AreaBoundary.GetBoundarySegmentArray();
		Option1.ReplacePoints(
			BoundaryDestSegment.HitSegment.EndPointIndex,
			BoundarySrcSegment.HitSegment.StartPointIndex,
			Path.GetPoints());
		Option1.ReverseVertexOrder();

		const float Area0 = Option0.CalculateArea();
		const float Area1 = Option1.CalculateArea();

		FLoopedSegmentArray2D& Bigger = Area0 > Area1 ? Option0 : Option1;

		AreaBoundary = MoveTemp(Bigger);
		TriangulateAreaAndSetInDynamicMesh();
	}

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FPolygonBoundary2D AreaBoundary;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DynamicMeshComponent->RegisterComponent();

		SetCurrentAsStartingPoint();
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		DynamicMeshComponent->DestroyComponent();
		DynamicMeshComponent = nullptr;
	}

	void SetCurrentAsStartingPoint()
	{
		const TArray<FVector2D> VertexPositions = [&]()
		{
			TArray<FVector2D> Ret;
			for (int32 AngleStep = 0; AngleStep < 16; AngleStep++)
			{
				const float ThisAngle = 2.f * UE_PI / 16.f * -AngleStep;
				Ret.Add(FVector2D{100.f * FMath::Cos(ThisAngle), 100.f * FMath::Sin(ThisAngle)});
			}
			return Ret;
		}();

		AreaBoundary.Replace(0, 0, VertexPositions);
		TriangulateAreaAndSetInDynamicMesh();
	}

	void TriangulateAreaAndSetInDynamicMesh()
	{
		UE::Geometry::FPlanarPolygonMeshGenerator Generator;
		Generator.Polygon = UE::Geometry::FPolygon2d{AreaBoundary.GetBoundarySegmentArray().GetPoints()};
		Generator.Generate();
		DynamicMeshComponent->GetMesh()->Copy(&Generator);
		DynamicMeshComponent->NotifyMeshUpdated();
	}
};
