// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Algo/Accumulate.h"


template <bool bLoop>
class TSegmentArray2D
{
public:
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
			const auto [Segment0Start, Segment0End] = *It;
			const auto [Segment1Start, Segment1End] = *ItPlusOne;
			const FVector2D Segment0 = (Segment0End - Segment0Start).GetSafeNormal();
			const FVector2D Segment1 = (Segment1End - Segment1Start).GetSafeNormal();
			Ret += FMath::Asin(FVector2D::CrossProduct(Segment0, Segment1));

			++It;
			++ItPlusOne;
		}

		return Ret;
	}

	template <bool bLoop2 = bLoop>
	std::enable_if_t<bLoop2, float> CalculateArea() const
	{
		// Shoelace formula
		return 0.5f * FMath::Abs(Algo::Accumulate(*this, 0.f, [](auto, const auto& Segment)
		{
			const auto& [SegmentStart, SegmentEnd] = Segment;
			return SegmentStart.X * SegmentEnd.Y - SegmentStart.Y * SegmentEnd.X;
		}));
	}

	void AddPoint(const FVector2D& Position)
	{
		Points.Add(Position);
	}

	void SetLastPoint(const FVector2D& NewPosition)
	{
		Points.Last() = NewPosition;
	}

	void ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints)
	{
		const int32 Count = FMath::Min(LastIndex - FirstIndex + 1, Points.Num());
		Points.RemoveAt(FirstIndex, Count);

		if (!NewPoints.IsEmpty())
		{
			Points.Insert(NewPoints.GetData(), NewPoints.Num(), FirstIndex);
		}
	}

	template <bool bLoop2 = bLoop>
	std::enable_if_t<bLoop2> ReplacePointsLooped(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints)
	{
		FirstIndex = FirstIndex % Points.Num();
		LastIndex = LastIndex % Points.Num();

		if (FirstIndex < LastIndex)
		{
			ReplacePoints(FirstIndex, LastIndex, NewPoints);
			return;
		}

		ReplacePoints(FirstIndex, Points.Num() - 1, NewPoints);
		ReplacePoints(0, LastIndex, {});
	}

	void RearrangeVertices(bool bClockwise)
	{
		const bool bCurrentWinding = CalculateNetAngleDelta() > 0.f;
		const bool bTargetWinding = bClockwise;

		if (bCurrentWinding != bTargetWinding)
		{
			std::reverse(Points.begin(), Points.end());
		}
	}

	void Empty()
	{
		Points.Empty();
	}

	class FSegmentConstIterator
	{
	public:
		using value_type = std::tuple<FVector2D, FVector2D>;

		FSegmentConstIterator(const TSegmentArray2D* InOrigin, bool bEnd = false)
			: Origin(InOrigin)
		{
			if (bEnd)
			{
				if constexpr (bLoop)
				{
					CurrentIndex = Origin->Points.Num();
				}
				else
				{
					CurrentIndex = Origin->Points.Num() - 1;
				}
			}
		}

		value_type operator*() const
		{
			if constexpr (bLoop)
			{
				if (CurrentIndex == Origin->Points.Num() - 1)
				{
					return {Origin->Points[CurrentIndex], Origin->Points[0]};
				}
			}

			return {Origin->Points[CurrentIndex], Origin->Points[CurrentIndex + 1]};
		}

		FSegmentConstIterator& operator++()
		{
			if constexpr (bLoop)
			{
				CurrentIndex = FMath::Min(Origin->Points.Num(), CurrentIndex + 1);
			}
			else
			{
				CurrentIndex = FMath::Min(Origin->Points.Num() - 1, CurrentIndex + 1);
			}

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
				if (CurrentIndex + 1 == Origin->Points.Num())
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
};


using FSegmentArray2D = TSegmentArray2D<false>;
using FLoopedSegmentArray2D = TSegmentArray2D<true>;
