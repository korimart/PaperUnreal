// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Core/ActorComponent2.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/PlanarPolygonMeshGenerator.h"
#include "AreaMeshComponent.generated.h"


UCLASS()
class UAreaBoundaryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoundaryChanged, const FLoopedSegmentArray2D&);
	FOnBoundaryChanged OnBoundaryChanged;

	void ResetToStartingBoundary(const FVector& Location)
	{
		const TArray<FVector2D> VertexPositions = [&]()
		{
			TArray<FVector2D> Ret;
			for (int32 AngleStep = 0; AngleStep < 16; AngleStep++)
			{
				const float ThisAngle = 2.f * UE_PI / 16.f * -AngleStep;
				Ret.Add(FVector2D{Location.X + 100.f * FMath::Cos(ThisAngle), Location.Y + 100.f * FMath::Sin(ThisAngle)});
			}
			return Ret;
		}();

		AreaBoundary.ReplacePoints(0, 0, VertexPositions);
		OnBoundaryChanged.Broadcast(AreaBoundary);
	}

	struct FExpansionResult
	{
		bool bAddedToTheLeftOfPath;
	};

	TOptional<FExpansionResult> ExpandByPath(const FSegmentArray2D& Path)
	{
		if (Path.IsValid())
		{
			if (TOptional<FLoopedSegmentArray2D::FUnionResult> UnionResult = AreaBoundary.Union(Path))
			{
				OnBoundaryChanged.Broadcast(AreaBoundary);
				return FExpansionResult{.bAddedToTheLeftOfPath = UnionResult->bUnionedToTheLeftOfPath};
			}
		}
		return {};
	}

	void ReduceByPath(const FSegmentArray2D& Path, bool bToTheLeftOfPath)
	{
		if (Path.IsValid())
		{
			AreaBoundary.Difference(Path, bToTheLeftOfPath);
			OnBoundaryChanged.Broadcast(AreaBoundary);
		}
	}

	const FLoopedSegmentArray2D& GetBoundary() const
	{
		return AreaBoundary;
	}

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary.IsInside(FVector2D{Point});
	}

	struct FPointOnBoundary
	{
		FSegment2D Segment;
		float Alpha;

		FVector2D GetPoint() const
		{
			return Segment.PointBetween(Alpha);
		}
	};

	TOptional<FPointOnBoundary> FindIntersection(const UE::Geometry::FSegment2d& Segment) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;

		if (TOptional<FIntersection> Found = AreaBoundary.FindIntersection(Segment))
		{
			FPointOnBoundary Ret;
			Ret.Segment = AreaBoundary[Found->SegmentIndex];
			Ret.Alpha = Found->Alpha;
			return Ret;
		}

		return {};
	}

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = AreaBoundary.FindClosestPointTo(Point);

		FPointOnBoundary Ret;
		Ret.Segment = AreaBoundary[Intersection.SegmentIndex];
		Ret.Alpha = Intersection.Alpha;
		return Ret;
	}

	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const
	{
		return AreaBoundary.Clip(Segment);
	}

private:
	FLoopedSegmentArray2D AreaBoundary;
};


UCLASS()
class UAreaMeshComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	template <typename LoopedSegmentArray2DType>
		requires std::is_convertible_v<LoopedSegmentArray2DType, FLoopedSegmentArray2D>
	void SetMeshByWorldBoundary(LoopedSegmentArray2DType&& WorldBoundary)
	{
		LastSetWorldBoundary = Forward<LoopedSegmentArray2DType>(WorldBoundary);

		FLoopedSegmentArray2D LocalBoundary = LastSetWorldBoundary;
		LocalBoundary.ApplyToEachPoint([&](FVector2D& Each) { Each = WorldToLocal2D(Each); });

		UE::Geometry::FPlanarPolygonMeshGenerator Generator;
		Generator.Polygon = UE::Geometry::FPolygon2d{LocalBoundary.GetPoints()};
		Generator.Generate();
		DynamicMeshComponent->GetMesh()->Copy(&Generator);
		DynamicMeshComponent->NotifyMeshUpdated();
	}

	void ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
	{
		DynamicMeshComponent->ConfigureMaterialSet(NewMaterialSet);
	}

	bool IsValid() const
	{
		return LastSetWorldBoundary.IsValid();
	}

	FSimpleMulticastDelegate& GetOnMeshChanged() const
	{
		return DynamicMeshComponent->OnMeshChanged;
	}

	struct FPointOnBoundary
	{
		FSegment2D Segment;
		float Alpha;

		FVector2D GetPoint() const
		{
			return Segment.PointBetween(Alpha);
		}
	};

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = LastSetWorldBoundary.FindClosestPointTo(Point);

		FPointOnBoundary Ret;
		Ret.Segment = LastSetWorldBoundary[Intersection.SegmentIndex];
		Ret.Alpha = Intersection.Alpha;
		return Ret;
	}

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FLoopedSegmentArray2D LastSetWorldBoundary;

	FVector2D WorldToLocal2D(const FVector2D& World2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(FVector{World2D, 0.f})};
	}

	FVector2D LocalToWorld2D(const FVector2D& Local2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().TransformPosition(FVector{Local2D, 0.f})};
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DynamicMeshComponent->RegisterComponent();
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		DynamicMeshComponent->DestroyComponent();
		DynamicMeshComponent = nullptr;
	}
};
