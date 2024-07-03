﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/PlanarPolygonMeshGenerator.h"
#include "AreaMeshComponent.generated.h"


UCLASS()
class UAreaMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	struct FPointOnBoundary
	{
		FSegment2D Segment;
		float Alpha;

		FVector2D GetPoint() const
		{
			return Segment.PointBetween(Alpha);
		}
	};

	FVector2D WorldToLocal2D(const FVector2D& World2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(FVector{World2D, 0.f})};
	}

	FVector2D LocalToWorld2D(const FVector2D& Local2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().TransformPosition(FVector{Local2D, 0.f})};
	}

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary.IsInside(WorldToLocal2D(FVector2D{Point}));
	}
	
	TOptional<FPointOnBoundary> FindIntersection(const UE::Geometry::FSegment2d& Segment) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;

		const UE::Geometry::FSegment2d LocalSegment
		{
			WorldToLocal2D(Segment.StartPoint()),
			WorldToLocal2D(Segment.EndPoint()),
		};
		
		if (TOptional<FIntersection> Found = AreaBoundary.FindIntersection(LocalSegment))
		{
			const FSegment2D& HitSegment = AreaBoundary[Found->SegmentIndex];

			FPointOnBoundary Ret;
			Ret.Segment = FSegment2D{LocalToWorld2D(HitSegment.StartPoint()), LocalToWorld2D(HitSegment.EndPoint()),};
			Ret.Alpha = Found->Alpha;
			return Ret;
		}

		return {};
	}

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = AreaBoundary.FindClosestPointTo(WorldToLocal2D(Point));
		const FSegment2D& HitSegment = AreaBoundary[Intersection.SegmentIndex];

		FPointOnBoundary Ret;
		Ret.Segment = FSegment2D{LocalToWorld2D(HitSegment.StartPoint()), LocalToWorld2D(HitSegment.EndPoint()),};
		Ret.Alpha = Intersection.Alpha;

		return Ret;
	}

	struct FExpansionResult
	{
		bool bAddedToTheLeftOfPath;
	};
	
	TOptional<FExpansionResult> ExpandByPath(FSegmentArray2D Path)
	{
		if (Path.IsValid())
		{
			Path.ApplyToEachPoint([this](FVector2D& Each) { Each = WorldToLocal2D(Each); });
			
			if (TOptional<FLoopedSegmentArray2D::FUnionResult> UnionResult = AreaBoundary.Union(Path))
			{
				TriangulateAreaAndSetInDynamicMesh();
				return FExpansionResult{.bAddedToTheLeftOfPath = UnionResult->bUnionedToTheLeftOfPath};
			}
		}
		return {};
	}
	
	void ReduceByPath(FSegmentArray2D Path, bool bToTheLeftOfPath)
	{
		if (Path.IsValid())
		{
			Path.ApplyToEachPoint([this](FVector2D& Each) { Each = WorldToLocal2D(Each); });
			AreaBoundary.Difference(Path, bToTheLeftOfPath);
			TriangulateAreaAndSetInDynamicMesh();
		}
	}

	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const
	{
		const UE::Geometry::FSegment2d LocalSegment
		{
			WorldToLocal2D(Segment.StartPoint()),
			WorldToLocal2D(Segment.EndPoint()),
		};

		const UE::Geometry::FSegment2d Clipped = AreaBoundary.Clip(LocalSegment);
		
		return {LocalToWorld2D(Clipped.StartPoint()), LocalToWorld2D(Clipped.EndPoint())};
	}
	
	void ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
	{
		DynamicMeshComponent->ConfigureMaterialSet(NewMaterialSet);
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
