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
	struct FHitSegment
	{
		FSegment2D Points;
		int32 StartPointIndex;
		int32 EndPointIndex;
	};

	struct FIntersection
	{
		FHitSegment HitSegment;
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
		for (const FSegment2D& Each : Segments)
		{
			if (Each.ContainsY(Point.Y))
			{
				const int32 IntersectionX = Each.StartPoint().X + (Point.Y - Each.StartPoint().Y) * Each.Slope();

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
		float ShortestDistance = TNumericLimits<float>::Max();
		for (auto [It, End] = std::tuple{Segments.begin(), Segments.end()}; It != End; ++It)
		{
			const FSegment2D& EachSegment = *It;
			const FSegment2D SegmentToPoint = EachSegment.Perp(Point);
			const float DistanceToPoint = SegmentToPoint.Length();

			if (ShortestDistance < DistanceToPoint)
			{
				ShortestDistance = DistanceToPoint;
				Ret.HitSegment.Points = EachSegment;
				Ret.HitSegment.StartPointIndex = It.GetStartPointIndex();
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.PointOfIntersection = SegmentToPoint.StartPoint();
			}
		}
		return Ret;
	}

	TOptional<FIntersection> FindIntersectionWithSegment(const UE::Geometry::FSegment2d& Segment) const
	{
		if (!Segments.IsValid())
		{
			return {};
		}

		for (auto [It, End] = std::tuple{Segments.begin(), Segments.end()}; It != End; ++It)
		{
			const FSegment2D& EachSegment = *It;
			if (TOptional<FVector2D> Intersection = EachSegment.Intersects(Segment))
			{
				FIntersection Ret;
				Ret.HitSegment.Points = EachSegment;
				Ret.HitSegment.StartPointIndex = It.GetStartPointIndex();
				Ret.HitSegment.EndPointIndex = It.GetEndPointIndex();
				Ret.PointOfIntersection = *Intersection;
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
