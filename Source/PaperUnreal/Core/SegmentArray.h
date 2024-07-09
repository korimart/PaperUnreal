// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentTypes.h"
#include "Utils.h"
#include "Algo/Accumulate.h"


struct FRay2D
{
	FVector2D Origin;
	FVector2D Direction;

	TOptional<std::tuple<double, double>> RayTrace(const UE::Geometry::FSegment2d& Segment) const
	{
		const FVector2D SegmentLengthedDirection = Segment.Direction * Segment.Length();
		const double RayDirCrossSegDir = FVector2D::CrossProduct(Direction, SegmentLengthedDirection);

		if (FMath::IsNearlyZero(RayDirCrossSegDir))
		{
			return {};
		}

		const FVector2D OriginToSegStart = Segment.StartPoint() - Origin;
		const double T = FVector2D::CrossProduct(OriginToSegStart, SegmentLengthedDirection) / RayDirCrossSegDir;
		const double U = FVector2D::CrossProduct(OriginToSegStart, Direction) / RayDirCrossSegDir;

		if ((FMath::IsNearlyZero(T) || T > 0.)
			&& (FMath::IsNearlyZero(U) || 0. < U)
			&& (FMath::IsNearlyEqual(U, 1.) || U < 1.))
		{
			return std::make_tuple(T, U);
		}

		return {};
	}
};


struct FSegment2D : UE::Geometry::FSegment2d
{
	using Super = UE::Geometry::FSegment2d;
	using Super::Super;

	FSegment2D(const Super& Other)
		: Super(Other)
	{
	}

	bool NearlyContainsX(double X) const
	{
		return IsNearlyLE(FMath::Min(StartPoint().X, EndPoint().X), X)
			&& IsNearlyLE(X, FMath::Max(StartPoint().X, EndPoint().X));
	}

	bool NearlyContainsY(double Y) const
	{
		return IsNearlyLE(FMath::Min(StartPoint().Y, EndPoint().Y), Y)
			&& IsNearlyLE(Y, FMath::Max(StartPoint().Y, EndPoint().Y));
	}

	float Slope() const
	{
		return (EndPoint().Y - StartPoint().Y) / (EndPoint().X - StartPoint().X);
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


template <bool>
class TSegmentArray2D;

using FSegmentArray2D = TSegmentArray2D<false>;
using FLoopedSegmentArray2D = TSegmentArray2D<true>;


template <typename T>
concept CSegmentArray2D = std::is_convertible_v<T, FSegmentArray2D>;

template <typename T>
concept CLoopedSegmentArray2D = std::is_convertible_v<T, FLoopedSegmentArray2D>;


template <bool bLoop>
class TSegmentArray2D
{
public:
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

	const TArray<FVector2D>& GetPoints() const &
	{
		return Points;
	}

	TArray<FVector2D> GetPoints() &&
	{
		return MoveTemp(Points);
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

	// TODO 어떤 함수는 마이너스 인덱스를 받고 어떤 함수는 안 받고 중구난방임
	// 그리고 GetLast 함수들이 따로 있음 전부 마이너스로 통일해야 됨
	int32 PositivePointIndex(int32 Index) const
	{
		return Index >= 0 ? Index : PointCount() + Index;
	}

	int32 PositiveSegmentIndex(int32 Index) const
	{
		return Index >= 0 ? Index : SegmentCount() + Index;
	}

	FVector2D GetPoint(int32 Index) const
	{
		return Points[PositivePointIndex(Index)];
	}

	FVector2D GetLastPoint(int32 IndexFromLast = 0) const
	{
		return Points[Points.Num() - IndexFromLast - 1];
	}

	FSegment2D GetLastSegment(int32 IndexFromLast = 0) const
	{
		return operator[](SegmentCount() - IndexFromLast - 1);
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

	void AddPoint(const FVector2D& Position)
	{
		Points.Add(Position);
	}

	void SetPoint(int32 Index, const FVector2D& NewPosition)
	{
		Points[PositivePointIndex(Index)] = NewPosition;
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

	void RemovePoints(int32 StartIndex, int32 LastIndex)
	{
		Points.RemoveAt(StartIndex, LastIndex - StartIndex + 1);
	}

	void SetSegment(int32 SegmentIndex, const UE::Geometry::FSegment2d& NewSegment)
	{
		SegmentIndex = PositiveSegmentIndex(SegmentIndex);
		const int32 PointIndex0 = SegmentIndexToStartPointIndex(SegmentIndex);
		const int32 PointIndex1 = SegmentIndexToEndPointIndex(SegmentIndex);
		SetPoint(PointIndex0, NewSegment.StartPoint());
		SetPoint(PointIndex1, NewSegment.EndPoint());
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

		TArray<double> IntersectingXes;
		for (const FSegment2D& Each : *this)
		{
			if (Each.NearlyContainsY(Point.Y))
			{
				const double IntersectionX = Each.StartPoint().X + (Point.Y - Each.StartPoint().Y) / Each.Slope();

				if (IsNearlyLE(Point.X, IntersectionX))
				{
					if (!IntersectingXes.FindByPredicate([&](double X)
					{
						return FMath::IsNearlyEqual(X, IntersectionX, UE_KINDA_SMALL_NUMBER);
					}))
					{
						IntersectingXes.Add(IntersectionX);
					}
				}
			}
		}

		return IntersectingXes.Num() % 2 == 1;
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
				Ret.Alpha = *Intersection;
				return Ret;
			}
		}

		return {};
	}

	TArray<FIntersection> FindAllIntersections(const UE::Geometry::FSegment2d& Segment) const
	{
		if (!IsValid())
		{
			return {};
		}

		TArray<FIntersection> Ret;
		for (int32 i = 0; i < SegmentCount(); i++)
		{
			const FSegment2D& EachSegment = operator[](i);
			if (TOptional<float> Intersection = EachSegment.Intersects(Segment))
			{
				Ret.Add({
					.SegmentIndex = i,
					.Alpha = *Intersection,
				});
			}
		}

		return Ret;
	}

	TOptional<FVector2D> Attach(const FVector2D& Point, const FVector2D& Direction) const
	{
		const FRay2D Ray{Point, Direction};

		double Distance = TNumericLimits<double>::Max();
		TOptional<FVector2D> Ret;
		for (FSegment2D Each : *this)
		{
			if (TOptional<std::tuple<double, double>> Hit = Ray.RayTrace(Each))
			{
				if (std::get<0>(*Hit) < Distance)
				{
					Distance = std::get<0>(*Hit);
					Ret = Each.PointBetween(std::get<1>(*Hit));
				}
			}
		}
		return Ret;
	}

	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const requires bLoop
	{
		if (!IsValid())
		{
			return Segment;
		}

		UE::Geometry::FSegment2d Ret = Segment;

		if (!IsInside(Segment.StartPoint()))
		{
			if (TOptional<FVector2D> AttachedPoint = Attach(Segment.StartPoint(), Segment.Direction))
			{
				Ret.SetStartPoint(*AttachedPoint);
			}
		}

		if (!IsInside(Segment.EndPoint()))
		{
			if (TOptional<FVector2D> AttachedPoint = Attach(Segment.EndPoint(), -Segment.Direction))
			{
				Ret.SetEndPoint(*AttachedPoint);
			}
		}

		return Ret;
	}

	auto Union(FSegmentArray2D Path) requires bLoop;

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
		Index = PositiveSegmentIndex(Index);
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

	auto UnionAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop;
};


struct FUnionResult
{
	FSegmentArray2D Path;
	bool bUnionedToTheLeftOfPath;
};


template <bool bLoop>
auto TSegmentArray2D<bLoop>::UnionAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop
{
	const FIntersection BoundarySrcSegment = FindClosestPointTo(Path.GetPoints()[0]);
	const FIntersection BoundaryDestSegment = FindClosestPointTo(Path.GetLastPoint());
	
	FLoopedSegmentArray2D SrcToDestReplaced = *this;
	SrcToDestReplaced.ReplacePointsBySegmentIndices(
		BoundarySrcSegment.SegmentIndex,
		BoundaryDestSegment.SegmentIndex,
		Path.GetPoints());

	FSegmentArray2D ReversedPath = Path;
	ReversedPath.ReverseVertexOrder();
	FLoopedSegmentArray2D DestToSrcReplaced = *this;
	DestToSrcReplaced.ReplacePointsBySegmentIndices(
		BoundaryDestSegment.SegmentIndex,
		BoundarySrcSegment.SegmentIndex,
		ReversedPath.GetPoints());

	const float CurrentArea = CalculateArea();
	const float Area0 = SrcToDestReplaced.CalculateArea();
	const float Area1 = DestToSrcReplaced.CalculateArea();

	if (CurrentArea > FMath::Max(Area0, Area1))
	{
		return TOptional<FUnionResult>{};
	}

	FSegmentArray2D& UsedPath = Area0 < Area1 ? ReversedPath : Path;
	Points = Area0 < Area1 ? MoveTemp(DestToSrcReplaced.Points) : MoveTemp(SrcToDestReplaced.Points);

	return TOptional<FUnionResult>{{.Path = MoveTemp(UsedPath), .bUnionedToTheLeftOfPath = Area0 >= Area1,}};
}


template <bool bLoop>
auto TSegmentArray2D<bLoop>::Union(FSegmentArray2D Path) requires bLoop
{
	struct FPathIntersection
	{
		int32 PathSegmentIndex;
		FVector2D Point;
	};

	// Segment의 양 지점이 Boundary 위에 정확하게 걸쳐져 있는 경우 Intersection Test에 실패할 수 있기 때문에 조금 늘려줌
	Path.SetPoint(0, Path.GetPoints()[0] - Path[0].Direction * UE_KINDA_SMALL_NUMBER);
	Path.SetPoint(-1, Path.GetLastPoint() + Path.GetLastSegmentDirection() * UE_KINDA_SMALL_NUMBER);

	TArray<FPathIntersection> AllIntersections;
	for (int32 i = 0; i < Path.SegmentCount(); i++)
	{
		for (const FIntersection& Each : FindAllIntersections(Path[i]))
		{
			AllIntersections.Add({
				.PathSegmentIndex = i,
				.Point = Each.Location(*this),
			});
		}
	}

	AllIntersections.Sort([&](const FPathIntersection& Left, const FPathIntersection& Right)
	{
		const double LeftDist = (Left.Point - Path.GetPoints()[0]).Length();
		const double RightDist = (Right.Point - Path.GetPoints()[0]).Length();
		return LeftDist < RightDist;
	});

	TArray<FUnionResult> Ret;
	for (int32 i = 0; i < AllIntersections.Num() - 1; i++)
	{
		const FPathIntersection& Intersection0 = AllIntersections[i];
		const FPathIntersection& Intersection1 = AllIntersections[i + 1];

		FSegmentArray2D Sub = Path.SubArray(Intersection0.PathSegmentIndex, Intersection1.PathSegmentIndex);
		Sub.SetPoint(0, Intersection0.Point);
		Sub.SetPoint(-1, Intersection1.Point);

		if (TOptional<FUnionResult> UnionResult = UnionAssumeTwoIntersections(MoveTemp(Sub)))
		{
			Ret.Add(MoveTemp(*UnionResult));
		}
	}

	return Ret;
}
