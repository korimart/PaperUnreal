// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentArray.h"
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
		FVector2D StartPoint;
		
		int32 EndPointIndex;
		FVector2D EndPoint;
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

		int32 IntersectionCount = 0;
		for (const auto& [SegmentStart, SegmentEnd] : Segments)
		{
			if (FMath::Min(SegmentStart.Y, SegmentEnd.Y) <= Point.Y
				&& Point.Y < FMath::Max(SegmentStart.Y, SegmentEnd.Y))
			{
				const float Slope = (SegmentEnd.X - SegmentStart.X) / (SegmentEnd.Y - SegmentStart.Y);
				const int32 IntersectionX = SegmentStart.X + (Point.Y - SegmentStart.Y) * Slope;

				if (Point.X < IntersectionX)
				{
					IntersectionCount++;
				}
			}
		}

		return IntersectionCount % 2 == 1;
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
				Ret.HitSegment.StartPoint = EachSegmentStart;
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.HitSegment.EndPoint = EachSegmentEnd;
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
				Ret.HitSegment.StartPoint = EachSegmentStart;
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.HitSegment.EndPoint = EachSegmentEnd;
				Ret.PointOfIntersection = FVector2D{Intersection};
				return Ret;
			}
		}

		return {};
	}
	
	void InsertPoints(int32 Index, const TArray<FVector2D>& NewPoints)
	{
		Segments.InsertPoints(Index, NewPoints);
	}

	void ReplacePoints(int32 FirstIndex, int32 LastIndex, const TArray<FVector2D>& NewPoints)
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

	FVector2D WorldToLocal2D(const FVector2D& World2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(FVector{World2D, 0.f})};
	}

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary.IsInside(WorldToLocal2D(FVector2D{Point}));
	}

	FIntersection FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		FIntersection Ret = AreaBoundary.FindClosestPointTo(WorldToLocal2D(Point));
		const FVector WorldPoint = GetOwner()->GetActorTransform().TransformPosition(FVector{Ret.PointOfIntersection, 0.f});
		Ret.PointOfIntersection = FVector2D{WorldPoint};
		return Ret;
	}

	void ExpandByCounterClockwisePath(FSegmentArray2D Path)
	{
		Path.ApplyToEachPoint([this](FVector2D& Each) { Each = WorldToLocal2D(Each); });
		
		const FIntersection BoundarySrcSegment = AreaBoundary.FindClosestPointTo(Path.GetPoints()[0]);
		const FIntersection BoundaryDestSegment = AreaBoundary.FindClosestPointTo(Path.GetLastPoint());

		if (BoundarySrcSegment.HitSegment.StartPointIndex == BoundaryDestSegment.HitSegment.StartPointIndex)
		{
			AreaBoundary.InsertPoints(BoundarySrcSegment.HitSegment.EndPointIndex, Path.GetPoints());
		}
		else
		{
			AreaBoundary.ReplacePoints(
				BoundarySrcSegment.HitSegment.EndPointIndex,
				BoundaryDestSegment.HitSegment.StartPointIndex,
				Path.GetPoints());
		}
		
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

		AreaBoundary.ReplacePoints(0, 0, VertexPositions);
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
