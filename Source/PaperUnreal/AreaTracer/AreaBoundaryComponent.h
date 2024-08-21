// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryProvider.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CoroutineMutex.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "AreaBoundaryComponent.generated.h"


UCLASS()
class UAreaBoundaryComponent : public UActorComponent2, public IAreaBoundaryProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<FLoopedSegmentArray2D> GetBoundary() const override { return AreaBoundary; }

	bool IsValid() const
	{
		return AreaBoundary.Get().IsValid();
	}

	void Reset()
	{
		AreaBoundary.SetValueNoComparison(FLoopedSegmentArray2D{});
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

		AreaBoundary.SetValueNoComparison(VertexPositions);
	}

	using FExpansionResult = FUnionResult;

	template <CSegmentArray2D SegmentArrayType>
	CAwaitable auto ExpandByPath(SegmentArrayType&& Path)
	{
		return ExpandByPathLocked(Forward<SegmentArrayType>(Path));
	}

	template <CSegmentArray2D SegmentArrayType>
	void ReduceByPath(SegmentArrayType&& Path)
	{
		if (Path.IsValid())
		{
			AreaBoundary.Modify([&](FLoopedSegmentArray2D& Boundary)
			{
				return Boundary.Difference(Forward<SegmentArrayType>(Path));
			});
		}
	}

	bool IsInside(const FVector& Point) const
	{
		return AreaBoundary.Get().IsInside(FVector2D{Point});
	}

	bool IsInside(UAreaBoundaryComponent* Other) const
	{
		if (const TOptional<FLoopedSegmentArray2D>& OtherBoundary = Other->GetBoundary().Get())
		{
			return AreaBoundary.Get().IsInside(*OtherBoundary);
		}
		return false;
	}

	FVector GetRandomPointInside() const
	{
		// 그냥 적당히 좋은 구현
		// 영역의 바운딩 박스를 가로 세로 10등분해서 랜덤 셀을 하나 고른다음에
		// 그 중심이 영역 안에 있으면 그걸 반환 아니면 거기서 가장 가까운 영역 내 위치 반환

		const FBox2D BoundingBox = AreaBoundary.Get().CalculateBoundingBox();
		const float CellWidth = BoundingBox.GetSize().X;
		const float CellHeight = BoundingBox.GetSize().Y;
		const int32 CellX = FMath::RandRange(0, 9);
		const int32 CellY = FMath::RandRange(0, 9);
		const float X = BoundingBox.Min.X + CellWidth / 2.f + CellWidth * CellX;
		const float Y = BoundingBox.Min.Y + CellHeight / 2.f + CellHeight * CellY;

		const FVector2D RandomCellCenter{X, Y};

		if (AreaBoundary.Get().IsInside(RandomCellCenter))
		{
			return FVector{RandomCellCenter, GetOwner()->GetActorLocation().Z};
		}

		return FVector{FindClosestPointOnBoundary2D(RandomCellCenter).GetPoint(), GetOwner()->GetActorLocation().Z};
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

		if (TOptional<FIntersection> Found = AreaBoundary.Get().FindIntersection(Segment))
		{
			FPointOnBoundary Ret;
			Ret.Segment = AreaBoundary.Get()[Found->SegmentIndex];
			Ret.Alpha = Found->Alpha;
			return Ret;
		}

		return {};
	}

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = AreaBoundary.Get().FindClosestPointTo(Point);

		FPointOnBoundary Ret;
		Ret.Segment = AreaBoundary.Get()[Intersection.SegmentIndex];
		Ret.Alpha = Intersection.Alpha;
		return Ret;
	}

	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const
	{
		return AreaBoundary.Get().Clip(Segment);
	}

private:
	TLiveData<FLoopedSegmentArray2D> AreaBoundary;

	TWeakCoroutine<TArray<FExpansionResult>> ExpandByPathLocked(FSegmentArray2D Path)
	{
		if (!Path.IsValid())
		{
			co_return TArray<FExpansionResult>{};
		}

		FCoroutineScopedLock ScopedLock;
		co_await ScopedLock.Lock(AreaBoundary.Mutex);

		TArray<FExpansionResult> Ret;
		AreaBoundary.ModifyAssumeLocked([&](FLoopedSegmentArray2D& Boundary)
		{
			Ret = Boundary.Union(MoveTemp(Path));
			const bool bNotify = Ret.Num() > 0;
			return bNotify;
		});
		co_return Ret;
	}
};
