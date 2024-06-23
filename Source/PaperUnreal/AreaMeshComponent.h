// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentArray.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/PlanarPolygonMeshGenerator.h"
#include "AreaMeshComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAreaMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	using FIntersection = FLoopedSegmentArray2D::FIntersection;

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

	FLoopedSegmentArray2D AreaBoundary;

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
		Generator.Polygon = UE::Geometry::FPolygon2d{AreaBoundary.GetPoints()};
		Generator.Generate();
		DynamicMeshComponent->GetMesh()->Copy(&Generator);
		DynamicMeshComponent->NotifyMeshUpdated();
	}
};
