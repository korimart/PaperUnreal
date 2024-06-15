// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


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

	bool IsValid() const
	{
		if constexpr (bLoop)
		{
			return Points.Num() >= 3;
		}

		return Points.Num() >= 2;
	}

	void ReplacePoints(int32 FirstIndex, int32 LastIndex, const TArray<FVector2D>& NewPoints)
	{
		const int32 Count = FMath::Min(LastIndex - FirstIndex + 1, Points.Num());
		Points.RemoveAt(FirstIndex, Count);
		Points.Insert(NewPoints, FirstIndex);
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
