// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentTypes.h"
#include "Algo/Accumulate.h"


struct FSegment2D : UE::Geometry::FSegment2d
{
	using Super = UE::Geometry::FSegment2d;
	using Super::Super;

	bool ContainsX(float X) const
	{
		return FMath::Min(StartPoint().X, EndPoint().X) <= X
			&& X < FMath::Max(StartPoint().X, EndPoint().X);
	}

	bool ContainsY(float Y) const
	{
		return FMath::Min(StartPoint().Y, EndPoint().Y) <= Y
			&& Y < FMath::Max(StartPoint().Y, EndPoint().Y);
	}

	float Slope() const
	{
		return (EndPoint().X - StartPoint().X) / (EndPoint().Y - StartPoint().Y);
	}

	FSegment2D Perp(const FVector2D& Point) const
	{
		return {PointBetween(ProjectUnitRange(Point)), Point};
	}

	TOptional<float> Intersects(const UE::Geometry::FSegment2d& Segment) const
	{
		FVector Intersection;
		if (FMath::SegmentIntersection2D(
			FVector{Segment.StartPoint(), 0.f},
			FVector{Segment.EndPoint(), 0.f},
			FVector{StartPoint(), 0.f},
			FVector{EndPoint(), 0.f},
			Intersection))
		{
			return ProjectUnitRange(FVector2D{Intersection});
		}

		return {};
	}

	friend bool operator==(const FSegment2D& Left, const FSegment2D& Right)
	{
		return Left.Center.Equals(Right.Center)
			&& Left.Direction.Equals(Right.Direction)
			&& FMath::IsNearlyEqual(Left.Extent, Right.Extent);
	}
};


template <bool bLoop>
class TSegmentArray2D
{
public:
	using FSegmentArray2D = TSegmentArray2D<false>;
	using FLoopedSegmentArray2D = TSegmentArray2D<true>;

	struct FIntersection
	{
		int32 SegmentIndex;
		float Alpha;

		FVector2D Location(const TSegmentArray2D& Array) const
		{
			return Array[SegmentIndex].PointBetween(Alpha);
		}
	};

	TSegmentArray2D(const TArray<FVector2D>& InitPoints = {})
		: Points(InitPoints)
	{
	}

	const TArray<FVector2D>& GetPoints() const
	{
		return Points;
	}

	int32 PointCount() const
	{
		return Points.Num();
	}

	int32 SegmentCount() const
	{
		if constexpr (bLoop)
		{
			return PointCount();
		}
		else
		{
			return PointCount() - 1;
		}
	}

	static int32 SegmentIndexToStartPointIndex(int32 Index)
	{
		return Index;
	}

	int32 SegmentIndexToEndPointIndex(int32 Index) const
	{
		if constexpr (bLoop)
		{
			if (Index == SegmentCount() - 1)
			{
				return 0;
			}
		}

		return Index + 1;
	}

	FVector2D GetLastPoint(int32 IndexFromLast = 0) const
	{
		return Points[Points.Num() - IndexFromLast - 1];
	}

	FVector2D GetLastSegmentDirection(int32 IndexFromLast = 0) const
	{
		const FVector2D Position0 = GetLastPoint(IndexFromLast + 1);
		const FVector2D Position1 = GetLastPoint(IndexFromLast);
		return (Position1 - Position0).GetSafeNormal();
	}

	bool IsValid() const
	{
		if constexpr (bLoop)
		{
			return Points.Num() >= 3;
		}

		return Points.Num() >= 2;
	}

	float CalculateNetAngleDelta() const
	{
		float Ret = 0.f;

		for (int32 i = 0; i < SegmentCount() - 2; i++)
		{
			const FSegment2D Segment0 = operator[](i);
			const FSegment2D Segment1 = operator[](i + 1);
			Ret += FMath::Asin(FVector2D::CrossProduct(Segment0.Direction, Segment1.Direction));
		}

		return Ret;
	}

	float CalculateArea() const requires bLoop
	{
		// Shoelace formula
		return 0.5f * FMath::Abs(Algo::TransformAccumulate(*this, [](const FSegment2D& Segment)
		{
			return Segment.StartPoint().X * Segment.EndPoint().Y - Segment.StartPoint().Y * Segment.EndPoint().X;
		}, 0.f));
	}

	bool IsStraight() const
	{
		for (int32 i = 0; i < SegmentCount() - 2; i++)
		{
			const FSegment2D Segment0 = operator[](i);
			const FSegment2D Segment1 = operator[](i + 1);

			if (!Segment0.Direction.Equals(Segment1.Direction))
			{
				return false;
			}
		}

		return true;
	}

	bool IsClockwise() const
	{
		return CalculateNetAngleDelta() > 0.f;
	}

	bool IsInside(const FVector2D& Point) const requires bLoop
	{
		if (!IsValid())
		{
			return false;
		}

		int32 IntersectionCount = 0;
		for (const FSegment2D& Each : *this)
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

	FIntersection FindClosestPointTo(const FVector2D& Point) const
	{
		FIntersection Ret{};
		float ShortestDistance = TNumericLimits<float>::Max();

		for (int32 i = 0; i < SegmentCount(); i++)
		{
			const FSegment2D& EachSegment = operator[](i);
			const FSegment2D SegmentToPoint = EachSegment.Perp(Point);
			const float DistanceToPoint = SegmentToPoint.Length();

			if (DistanceToPoint < ShortestDistance)
			{
				ShortestDistance = DistanceToPoint;
				Ret.SegmentIndex = i;
				Ret.Alpha = EachSegment.ProjectUnitRange(SegmentToPoint.StartPoint());
			}
		}

		return Ret;
	}
	
	TOptional<FIntersection> FindIntersection(const UE::Geometry::FSegment2d& Segment) const
	{
		if (!IsValid())
		{
			return {};
		}

		for (int32 i = 0; i < SegmentCount(); i++)
		{
			const FSegment2D& EachSegment = operator[](i);
			if (TOptional<float> Intersection = EachSegment.Intersects(Segment))
			{
				FIntersection Ret;
				Ret.SegmentIndex = i;
				Ret.Alpha = Intersection;
				return Ret;
			}
		}

		return {};
	}

	void AddPoint(const FVector2D& Position)
	{
		Points.Add(Position);
	}

	void SetPoint(int32 Index, const FVector2D& NewPosition)
	{
		Points[Index] = NewPosition;
	}

	void SetLastPoint(const FVector2D& NewPosition)
	{
		Points.Last() = NewPosition;
	}

	void InsertPoints(int32 Index, TArrayView<const FVector2D> NewPoints)
	{
		if (!NewPoints.IsEmpty())
		{
			Points.Insert(NewPoints.GetData(), NewPoints.Num(), Index);
		}
	}

	template <typename... ArgTypes> requires !bLoop
	void ReplacePoints(ArgTypes&&... Args)
	{
		ReplacePointsNoLoop(Forward<ArgTypes>(Args)...);
	}

	void ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints) requires bLoop
	{
		FirstIndex = FirstIndex % FMath::Max(Points.Num(), 1);
		LastIndex = LastIndex % FMath::Max(Points.Num(), 1);

		if (FirstIndex <= LastIndex)
		{
			ReplacePointsNoLoop(FirstIndex, LastIndex, NewPoints);
			return;
		}

		ReplacePointsNoLoop(FirstIndex, Points.Num() - 1, NewPoints);
		ReplacePointsNoLoop(0, LastIndex, {});
	}

	struct FUnionResult
	{
		bool bUnionedToTheLeftOfPath;
	};

	TOptional<FUnionResult> Union(FSegmentArray2D Path) requires bLoop
	{
		const FIntersection BoundarySrcSegment = FindClosestPointTo(Path.GetPoints()[0]);
		const FIntersection BoundaryDestSegment = FindClosestPointTo(Path.GetLastPoint());

		FLoopedSegmentArray2D SrcToDestReplaced = *this;
		SrcToDestReplaced.ReplacePointsBySegmentIndices(
			BoundarySrcSegment.SegmentIndex,
			BoundaryDestSegment.SegmentIndex,
			Path.GetPoints());

		Path.ReverseVertexOrder();
		FLoopedSegmentArray2D DestToSrcReplaced = *this;
		DestToSrcReplaced.ReplacePointsBySegmentIndices(
			BoundaryDestSegment.SegmentIndex,
			BoundarySrcSegment.SegmentIndex,
			Path.GetPoints());

		const float CurrentArea = CalculateArea();
		const float Area0 = SrcToDestReplaced.CalculateArea();
		const float Area1 = DestToSrcReplaced.CalculateArea();

		if (CurrentArea > FMath::Max(Area0, Area1))
		{
			return {};
		}

		Points = Area0 < Area1 ? MoveTemp(DestToSrcReplaced.Points) : MoveTemp(SrcToDestReplaced.Points);

		return FUnionResult{.bUnionedToTheLeftOfPath = Area0 >= Area1,};
	}

	void Difference(FSegmentArray2D Path, bool bRemoveToTheLeftOfPath) requires bLoop
	{
		if (bRemoveToTheLeftOfPath)
		{
			Path.ReverseVertexOrder();
		}
		
		const FIntersection BoundarySrcSegment = FindClosestPointTo(Path.GetPoints()[0]);
		const FIntersection BoundaryDestSegment = FindClosestPointTo(Path.GetLastPoint());

		ReplacePointsBySegmentIndices(
			BoundarySrcSegment.SegmentIndex,
			BoundaryDestSegment.SegmentIndex,
			Path.GetPoints());
	}

	void ReverseVertexOrder()
	{
		std::reverse(Points.begin(), Points.end());
	}

	void Empty()
	{
		Points.Empty();
	}

	template <typename FuncType>
	void ApplyToEachPoint(FuncType&& Func)
	{
		for (auto& Each : Points)
		{
			Func(Each);
		}
	}

	FSegmentArray2D SubArray(int32 FirstSegmentIndex, int32 LastSegmentIndex) const
	{
		const int32 FirstPointIndex = SegmentIndexToStartPointIndex(FirstSegmentIndex);
		const int32 LastPointIndex = FMath::Max(FirstPointIndex, SegmentIndexToEndPointIndex(LastSegmentIndex));
		return {TArray{&Points[FirstPointIndex], LastPointIndex - FirstPointIndex + 1}};
	}

	FSegment2D operator[](int32 Index) const
	{
		return
		{
			Points[SegmentIndexToStartPointIndex(Index)],
			Points[SegmentIndexToEndPointIndex(Index)],
		};
	}

	class FSegmentConstIterator
	{
	public:
		using value_type = FSegment2D;

		FSegmentConstIterator(const TSegmentArray2D* InOrigin, int32 Index)
			: Origin(InOrigin), CurrentIndex(Index)
		{
		}

		value_type operator*() const
		{
			return Origin->operator[](CurrentIndex);
		}

		FSegmentConstIterator& operator++()
		{
			CurrentIndex++;
			return *this;
		}

		friend bool operator==(const FSegmentConstIterator& Left, const FSegmentConstIterator& Right)
		{
			return Left.CurrentIndex == Right.CurrentIndex;
		}

		friend bool operator!=(const FSegmentConstIterator& Left, const FSegmentConstIterator& Right)
		{
			return !(Left == Right);
		}

	private:
		const TSegmentArray2D* Origin;
		int32 CurrentIndex = 0;
	};

	FSegmentConstIterator begin() const
	{
		return {this, 0};
	}

	FSegmentConstIterator end() const
	{
		return {this, SegmentCount()};
	}

private:
	TArray<FVector2D> Points;

	void ReplacePointsNoLoop(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints)
	{
		const int32 Count = FMath::Min(LastIndex - FirstIndex + 1, Points.Num());
		Points.RemoveAt(FirstIndex, Count);
		InsertPoints(FirstIndex, NewPoints);
	}

	void ReplacePointsBySegmentIndices(int32 StartSegment, int32 EndSegment, const TArray<FVector2D>& NewPoints)
	{
		if (StartSegment == EndSegment)
		{
			InsertPoints(SegmentIndexToEndPointIndex(StartSegment), NewPoints);
		}
		else
		{
			ReplacePoints(
				SegmentIndexToEndPointIndex(StartSegment),
				SegmentIndexToStartPointIndex(EndSegment),
				NewPoints);
		}
	};
};


using FSegmentArray2D = TSegmentArray2D<false>;
using FLoopedSegmentArray2D = TSegmentArray2D<true>;
