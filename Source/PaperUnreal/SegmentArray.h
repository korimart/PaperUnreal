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

	TOptional<FVector2D> Intersects(const UE::Geometry::FSegment2d& Segment) const
	{
		FVector Intersection;
		if (FMath::SegmentIntersection2D(
			FVector{Segment.StartPoint(), 0.f},
			FVector{Segment.EndPoint(), 0.f},
			FVector{StartPoint(), 0.f},
			FVector{EndPoint(), 0.f},
			Intersection))
		{
			return FVector2D{Intersection};
		}

		return {};
	}
};


template <bool bLoop>
class TSegmentArray2D
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

		auto It = begin();
		auto ItPlusOne = begin();
		++ItPlusOne;
		auto End = end();

		while (ItPlusOne != End)
		{
			const FSegment2D Segment0 = *It;
			const FSegment2D Segment1 = *ItPlusOne;
			Ret += FMath::Asin(FVector2D::CrossProduct(Segment0.Direction, Segment1.Direction));

			++It;
			++ItPlusOne;
		}

		return Ret;
	}

	template <bool bLoop2 = bLoop>
	std::enable_if_t<bLoop2, float> CalculateArea() const
	{
		// Shoelace formula
		return 0.5f * FMath::Abs(Algo::TransformAccumulate(*this, [](const auto& Segment)
		{
			const auto& [SegmentStart, SegmentEnd] = Segment;
			return SegmentStart.X * SegmentEnd.Y - SegmentStart.Y * SegmentEnd.X;
		}, 0.f));
	}

	bool IsStraight() const
	{
		auto It = begin();
		auto ItPlusOne = begin();
		++ItPlusOne;
		auto End = end();

		while (ItPlusOne != End)
		{
			const FSegment2D Segment0 = *It;
			const FSegment2D Segment1 = *ItPlusOne;

			if (!Segment0.Direction.Equals(Segment1.Direction))
			{
				return false;
			}

			++It;
			++ItPlusOne;
		}

		return true;
	}

	bool IsClockwise() const
	{
		return CalculateNetAngleDelta() > 0.f;
	}

	template <bool bLoop2 = bLoop>
	std::enable_if_t<bLoop2, bool> IsInside(const FVector2D& Point) const
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
		for (auto [It, End] = std::tuple{begin(), end()}; It != End; ++It)
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

	TOptional<FIntersection> FindIntersection(const UE::Geometry::FSegment2d& Segment) const
	{
		if (!IsValid())
		{
			return {};
		}

		for (auto [It, End] = std::tuple{begin(), end()}; It != End; ++It)
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
	
	void AddPoint(const FVector2D& Position)
	{
		Points.Add(Position);
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

	template <bool bLoop2 = bLoop, typename... ArgTypes>
	std::enable_if_t<!bLoop2> ReplacePoints(ArgTypes&&... Args)
	{
		ReplacePointsNoLoop(Forward<ArgTypes>(Args)...);
	}

	template <bool bLoop2 = bLoop>
	std::enable_if_t<bLoop2> ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints)
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

	FSegment2D operator[](int32 Index) const
	{
		if constexpr (bLoop)
		{
			if (Index == SegmentCount() - 1)
			{
				return {Points[Index], Points[0]};
			}
		}

		return {Points[Index], Points[Index + 1]};
	}

	class FSegmentConstIterator
	{
	public:
		using value_type = FSegment2D;

		FSegmentConstIterator(const TSegmentArray2D* InOrigin, bool bEnd = false)
			: Origin(InOrigin)
		{
			if (bEnd)
			{
				CurrentIndex = Origin->SegmentCount();
			}
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

		int32 GetStartPointIndex() const
		{
			return CurrentIndex;
		}

		int32 GetEndPointIndex() const
		{
			if constexpr (bLoop)
			{
				if (CurrentIndex == Origin->SegmentCount() - 1)
				{
					return 0;
				}
			}

			return CurrentIndex + 1;
		}

	private:
		const TSegmentArray2D* Origin;
		int32 CurrentIndex = 0;
	};

	FSegmentConstIterator begin() const
	{
		return {this};
	}

	FSegmentConstIterator end() const
	{
		return {this, true};
	}

private:
	TArray<FVector2D> Points;

	void ReplacePointsNoLoop(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints)
	{
		const int32 Count = FMath::Min(LastIndex - FirstIndex + 1, Points.Num());
		Points.RemoveAt(FirstIndex, Count);
		InsertPoints(FirstIndex, NewPoints);
	}
};


using FSegmentArray2D = TSegmentArray2D<false>;
using FLoopedSegmentArray2D = TSegmentArray2D<true>;
