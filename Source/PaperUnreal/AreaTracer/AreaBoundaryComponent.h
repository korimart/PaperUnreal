﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryProvider.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "AreaBoundaryComponent.generated.h"


UCLASS()
class UAreaBoundaryComponent : public UActorComponent2, public IAreaBoundaryProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TLiveData<FLoopedSegmentArray2D>> GetBoundary() override
	{
		return AreaBoundary;
	}

	bool IsValid() const
	{
		return AreaBoundary->IsValid();
	}

	void Reset()
	{
		AreaBoundary.SetValueAlways(FLoopedSegmentArray2D{});
	}

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

		AreaBoundary.SetValueAlways(VertexPositions);
	}

	using FExpansionResult = FUnionResult;

	template <CSegmentArray2D SegmentArrayType>
	TArray<FExpansionResult> ExpandByPath(SegmentArrayType&& Path)
	{
		if (!Path.IsValid())
		{
			return {};
		}
		
		if (TArray<FUnionResult> UnionResult
			= AreaBoundary->Union(Forward<SegmentArrayType>(Path)); !UnionResult.IsEmpty())
		{
			AreaBoundary.Notify();

			TArray<FExpansionResult> Ret;
			for (FUnionResult& Each : UnionResult)
			{
				Ret.Add(MoveTemp(Each));
			}

			return Ret;
		}
		
		return {};
	}

	template <CSegmentArray2D SegmentArrayType>
	void ReduceByPath(SegmentArrayType&& Path)
	{
		if (Path.IsValid())
		{
			AreaBoundary->Difference(Forward<SegmentArrayType>(Path));
			AreaBoundary.Notify();
		}
	}

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary->IsInside(FVector2D{Point});
	}
	
	bool IsInside(UAreaBoundaryComponent* Other) const
	{
		if (const TOptional<FLoopedSegmentArray2D>& OtherBoundary = Other->GetBoundary().Get())
		{
			return AreaBoundary->IsInside(*OtherBoundary);
		}
		return false;
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

		if (TOptional<FIntersection> Found = AreaBoundary->FindIntersection(Segment))
		{
			FPointOnBoundary Ret;
			Ret.Segment = (*AreaBoundary)[Found->SegmentIndex];
			Ret.Alpha = Found->Alpha;
			return Ret;
		}

		return {};
	}

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = AreaBoundary->FindClosestPointTo(Point);

		FPointOnBoundary Ret;
		Ret.Segment = (*AreaBoundary)[Intersection.SegmentIndex];
		Ret.Alpha = Intersection.Alpha;
		return Ret;
	}

	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const
	{
		return AreaBoundary->Clip(Segment);
	}

private:
	TLiveData<FLoopedSegmentArray2D> AreaBoundary;
};