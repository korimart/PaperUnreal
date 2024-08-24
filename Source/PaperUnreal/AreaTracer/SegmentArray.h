// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentTypes.h"
#include "Algo/Accumulate.h"
#include "Algo/MaxElement.h"
#include "PaperUnreal/GameFramework2/Utils.h"


/**
 * 특정 지점에서 출발해 특정 방향으로 무한히 나아가는 레이를 나타내는 클래스
 */
struct FRay2D
{
	FVector2D Origin;
	FVector2D Direction;

	/**
	 * Ray와 Segment가 충돌하는지 검사합니다
	 * 
	 * @return 충돌했으면 충돌 지점의 좌표를 std::tuple로 반환
	 */
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


/**
 * 2차원 Segment를 나타내는 클래스
 */
struct FSegment2D : UE::Geometry::FSegment2d
{
	using Super = UE::Geometry::FSegment2d;
	using Super::Super;

	FSegment2D(const Super& Other)
		: Super(Other)
	{
	}

	/**
	 * Segment가 차지하는 X 좌표 범위 내에 파라미터로 주어진 X가 존재하는지 체크합니다.
	 * 양 경계 부분에서는 floating point 오차를 고려해 Nearly로 검사합니다.
	 */
	bool NearlyContainsX(double X) const
	{
		return IsNearlyLE(FMath::Min(StartPoint().X, EndPoint().X), X)
			&& IsNearlyLE(X, FMath::Max(StartPoint().X, EndPoint().X));
	}

	/**
	 * Segment가 차지하는 Y 좌표 범위 내에 파라미터로 주어진 Y가 존재하는지 체크합니다.
	 * 양 경계 부분에서는 floating point 오차를 고려해 Nearly로 검사합니다.
	 */
	bool NearlyContainsY(double Y) const
	{
		return IsNearlyLE(FMath::Min(StartPoint().Y, EndPoint().Y), Y)
			&& IsNearlyLE(Y, FMath::Max(StartPoint().Y, EndPoint().Y));
	}

	/**
	 * 이 Segment의 기울기를 반환합니다.
	 */
	float Slope() const
	{
		return (EndPoint().Y - StartPoint().Y) / (EndPoint().X - StartPoint().X);
	}

	/**
	 * 이 Segment에서 수직으로 출발하여 Point에 도착하는 Segment를 반환합니다.
	 */
	FSegment2D Perp(const FVector2D& Point) const
	{
		return {PointBetween(ProjectUnitRange(Point)), Point};
	}

	/**
	 * 이 Segment와 파라미터로 주어진 Segment의 교차지점을 이 Segment 위의 알파(0에서 1 사이의 값)로 반환합니다.
	 */
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

/**
 * FSegment2D들의 배열로, Segment들을 이어 붙여서 만든 일종의 Path를 나타내는 클래스
 */
using FSegmentArray2D = TSegmentArray2D<false>;

/**
 * FSegment2D들의 배열로, Segment들을 이어 붙여 만든 Path의 시작점과 끝점을 연결한
 * 일종의 Boundary를 나타내는 클래스
 */
using FLoopedSegmentArray2D = TSegmentArray2D<true>;


template <typename T>
concept CSegmentArray2D = std::is_convertible_v<T, FSegmentArray2D>;

template <typename T>
concept CLoopedSegmentArray2D = std::is_convertible_v<T, FLoopedSegmentArray2D>;


/**
 * FSegment2D들의 배열
 * 내부적으로 TArray이며 Segment들에 대한 여러가지 편의 함수들을 제공합니다.
 * 
 * @tparam bLoop 시작점과 끝점을 이어붙일 것인지 여부
 */
template <bool bLoop>
class TSegmentArray2D
{
public:
	struct FIntersection
	{
		/**
		 * 이 배열 내의 인덱스
		 */
		int32 SegmentIndex;

		/**
		 * Segment 상의 0에서 1사이의 좌표
		 */
		float Alpha;

		/**
		 * 위치로 변환합니다. 파라미터로 주어지는 Segment 배열은 이 클래스를 생산한 배열이어야 함
		 */
		FVector2D Location(const TSegmentArray2D& Array) const
		{
			return Array[SegmentIndex].PointBetween(Alpha);
		}
	};

	TSegmentArray2D(const TArray<FVector2D>& InitPoints = {})
		: Points(InitPoints)
	{
	}

	TSegmentArray2D(TArray<FVector2D>&& InitPoints)
		: Points(MoveTemp(InitPoints))
	{
	}

	/**
	 * Point 배열에 대해 파이썬과 같은 마이너스 기반 인덱스를 C++ 양의 인덱스로 변환합니다. 
	 */
	int32 PositivePointIndex(int32 Index) const
	{
		return Index >= 0 ? Index : PointCount() + Index;
	}

	/**
	 * Segment 배열에 대해 파이썬과 같은 마이너스 기반 인덱스를 C++ 양의 인덱스로 변환합니다. 
	 */
	int32 PositiveSegmentIndex(int32 Index) const
	{
		return Index >= 0 ? Index : SegmentCount() + Index;
	}

	/**
	 * Segment들을 구성하는 점들을 중복 없이 반환합니다.
	 * (두 Segment가 공유하는 점이 중복으로 두 번 들어가 있지 않다는 뜻)
	 */
	const TArray<FVector2D>& GetPoints() const
	{
		return Points;
	}

	/**
	 * Segment들을 구성하는 점들의 개수를 중복 없이 반환합니다.
	 * (두 Segment가 공유하는 점을 중복으로 두 번 세지 않는다는 뜻)
	 */
	int32 PointCount() const
	{
		return Points.Num();
	}

	/**
	 * Segment의 개수를 반환합니다.
	 */
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

	/**
	 * Segment의 인덱스를 받아서 Segment의 시작점에 대한 Point 인덱스를 반환합니다.
	 */
	static int32 SegmentIndexToStartPointIndex(int32 Index)
	{
		return Index;
	}

	/**
	 * Segment의 인덱스를 받아서 Segment의 끝점에 대한 Point 인덱스를 반환합니다.
	 */
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

	FVector2D GetPoint(int32 Index) const
	{
		return Points[PositivePointIndex(Index)];
	}

	FVector2D GetSegmentDirection(int32 Index) const
	{
		return operator[](Index).Direction;
	}

	/**
	 * 현재 FSegment2DArray가 기하학적으로 말이 되는 상태인지 반환합니다.
	 * Loop일 경우 - 점이 최소 3개 있어야 2차원 도형(삼각형)을 구성할 수 있음
	 * Loop이 아닐 경우 - 점이 최소 2개 있어야 최소 하나의 Segment를 구성할 수 있음
	 */
	bool IsValid() const
	{
		if constexpr (bLoop)
		{
			return Points.Num() >= 3;
		}
		else
		{
			return Points.Num() >= 2;
		}
	}

	/**
	 * 새 점을 추가하여 기존 마지막 점에서 새 점을 잇는 Segment를 추가합니다.
	 */
	void AddPoint(const FVector2D& Position)
	{
		Points.Add(Position);
	}

	/**
	 * 점의 위치를 변경합니다.
	 */
	void SetPoint(int32 Index, const FVector2D& NewPosition)
	{
		Points[PositivePointIndex(Index)] = NewPosition;
	}

	/**
	 * Segment 배열의 중간에 점들을 추가하고 이어서 새 Segment들을 삽입합니다.
	 *
	 * @param Index 점들을 추가할 위치의 Point Index
	 */
	void InsertPoints(int32 Index, TArrayView<const FVector2D> NewPoints)
	{
		if (!NewPoints.IsEmpty())
		{
			Points.Insert(NewPoints.GetData(), NewPoints.Num(), PositivePointIndex(Index));
		}
	}

	/**
	 * (No Loop 버전)
	 * [FirstIndex, LastIndex]의 점들을 새 점으로 교체하고 이어서 Segment들을 변경합니다.
	 */
	void ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints) requires !bLoop
	{
		ReplacePointsNoLoop(FirstIndex, LastIndex, NewPoints);
	}

	/**
	 * (Loop 버전)
	 * [FirstIndex, LastIndex]의 점들을 새 점으로 교체하고 이어서 Segment들을 변경합니다.
	 */
	void ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints) requires bLoop;

	/**
	 * [FirstIndex, LastIndex]의 점들을 제거하고 이어서 Segment들을 변경합니다.
	 */
	void RemovePoints(int32 StartIndex, int32 LastIndex)
	{
		Points.RemoveAt(StartIndex, LastIndex - StartIndex + 1);
	}

	/**
	 * Segment를 교체합니다. 기존에 이웃하던 Segment들은 새 Segment와 이어집니다.
	 */
	void SetSegment(int32 SegmentIndex, const UE::Geometry::FSegment2d& NewSegment)
	{
		SegmentIndex = PositiveSegmentIndex(SegmentIndex);
		const int32 PointIndex0 = SegmentIndexToStartPointIndex(SegmentIndex);
		const int32 PointIndex1 = SegmentIndexToEndPointIndex(SegmentIndex);
		SetPoint(PointIndex0, NewSegment.StartPoint());
		SetPoint(PointIndex1, NewSegment.EndPoint());
	}

	/**
	 * Segment들을 Path로 생각해서 처음부터 끝까지 따라 올라갈 때 발생하는 알짜회전각을 반화합니다.
	 * 
	 * 예를 들어서 첫번째 Segment에서 두번째 Segment가 30도 각도로 이어져있고
	 * 두번째 Segment와 세번째 Segment가 -50도 각도로 이어져있으면 반환되는 회전각은 -20도
	 */
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

	/**
	 * Segment들이 이루는 영역의 넓이를 반환합니다.
	 */
	float CalculateArea() const requires bLoop
	{
		// Shoelace formula
		return 0.5f * FMath::Abs(Algo::TransformAccumulate(*this, [](const FSegment2D& Segment)
		{
			return Segment.StartPoint().X * Segment.EndPoint().Y - Segment.StartPoint().Y * Segment.EndPoint().X;
		}, 0.f));
	}

	/**
	 * 모든 Segment들을 포함하는 가장 작은 Bounding Box를 반환합니다.
	 */
	FBox2D CalculateBoundingBox() const
	{
		return {Points};
	}

	/**
	 * Segment들이 모두 일자로 이어져 있는지 반환합니다.
	 * (하나의 Segment로 단순화할 수 있는지 여부와 같음)
	 */
	bool IsStraight() const;

	/**
	 * Segment들이 대체로 시계방향을 이루는지 여부
	 */
	bool IsClockwise() const
	{
		return CalculateNetAngleDelta() > 0.f;
	}

	/**
	 * 파라미터로 주어진 점이 Segment들이 이루는 영역 안에 존재하는지 여부를 반환합니다.
	 */
	bool IsInside(const FVector2D& Point) const requires bLoop;

	/**
	 * 파라미터로 주어진 Segment 배열이 이 배열의 Segment들이 이루는 영역 안에 존재하는지 여부를 반환합니다.
	 */
	bool IsInside(const TSegmentArray2D& Other) const requires bLoop;

	/**
	 * 파라미터로 주어진 점과 가장 가까운 Segment 상의 점을 반환합니다.
	 */
	FIntersection FindClosestPointTo(const FVector2D& Point) const;

	/**
	 * 파라미터로 주어진 Segment와의 교차지점 중에서 아무거나 반환합니다.
	 */
	TOptional<FIntersection> FindIntersection(const UE::Geometry::FSegment2d& Segment) const;

	/**
	 * 파라미터로 주어진 Segment와의 교차지점을 모두 반환합니다.
	 * 평행인 Segment가 있어서 교차지점이 무수히 많을 경우 그 중에서 하나만 반환합니다.
	 */
	TArray<FIntersection> FindAllIntersections(const UE::Geometry::FSegment2d& Segment) const;

	/**
	 * 파라미터로 주어진 점을 파라미터로 주어진 방향으로 이동시킬 때
	 * 가장 처음으로 이 Segment 배열과 만나는 점을 반환합니다.
	 */
	TOptional<FVector2D> Attach(const FVector2D& Point, const FVector2D& Direction) const;

	/**
	 * 파라미터로 주어진 Segment 중에서 이 Segment 배열이 이루는 영역 바깥으로 빠져나간 부분을 제거한 Segment를 반환합니다.
	 */
	UE::Geometry::FSegment2d Clip(const UE::Geometry::FSegment2d& Segment) const requires bLoop;

	/**
	 * 이 Segment 배열이 이루는 영역과 파라미터로 주어진 Segment 배열이 교차하여 이루는 새 영역들을 포함하도록 Segment 배열을 수정합니다.
	 *
	 * 영역의 확장은 Area와 Path가 두 번 이상 바깥으로 교차한 경우에 발생합니다.
	 * 영역 안쪽으로 들어갔다 나온 경우에는 두 번 교차하지만 확장할 영역이 없습니다. 즉 영역 안쪽에서 바깥쪽으로 나갔다가 들어온 경우에만 발생합니다.
	 * 
	 * 이렇게 파라미터로 주어진 Segment 배열을 확장에 성공한 Segment의 배열로 쪼개서 FUnionResult의 배열을 반환합니다.
	 * 예를 들어서 영역 안에서 나갔다 들어왔다 나갔다 들어왔다 한 경우 교차 지점은 총 4곳, 확장 성공 영역은 두 곳이며
	 * 확장 성공한 Path만 짤라서 FUnionResult 두 개를 반환합니다.
	 *
	 * FUnionResult로 반환되는 Path는 영역을 구성하는 Segment들과 같은 방향으로 정렬하여 반환됩니다.
	 * (기본적으로 영역은 항상 반시계방향 orientation을 유지하고 있으며 확장 이후에도 마찬가지)
	 */
	template <CSegmentArray2D SegmentArrayType>
	auto Union(SegmentArrayType&& Path) requires bLoop;

	/**
	 * Union과 비슷한 방식으로 Path가 Area에 안쪽으로 교차한 부분을 영역에서 잘라냅니다.
	 * 잘라낸 부분이 있는지 여부만 반환합니다.
	 */
	template <CSegmentArray2D SegmentArrayType>
	bool Difference(SegmentArrayType&& Path) requires bLoop;

	void ReverseVertexOrder()
	{
		std::reverse(Points.begin(), Points.end());
	}

	void Empty()
	{
		Points.Empty();
	}

	/**
	 * Segment 배열을 구성하는 각 점에 대해 함수를 호출합니다.
	 */
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

	/**
	 * Index 위치의 Segment를 반환합니다.
	 */
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

	/**
	 * Path를 2번 교차하는 Path들로 쪼개서 반환합니다.
	 * 예를 들어 영역에서 나갔다 들어왔다 나간 경우 (나감-들어옴), (들어옴-나감)의 두 부분으로 쪼개서 반환
	 */
	TArray<FSegmentArray2D> SplitIntoCleanPaths(FSegmentArray2D Path);

	auto UnionAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop;
	void DifferenceAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop;
};


struct FUnionResult
{
	FSegmentArray2D CorrectlyAlignedPath;
};


template <bool bLoop>
void TSegmentArray2D<bLoop>::ReplacePoints(int32 FirstIndex, int32 LastIndex, TArrayView<const FVector2D> NewPoints) requires bLoop
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

template <bool bLoop>
bool TSegmentArray2D<bLoop>::IsStraight() const
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

template <bool bLoop>
bool TSegmentArray2D<bLoop>::IsInside(const FVector2D& Point) const requires bLoop
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

template <bool bLoop>
bool TSegmentArray2D<bLoop>::IsInside(const TSegmentArray2D& Other) const requires bLoop
{
	for (const FVector2D& Each : Other.GetPoints())
	{
		if (!IsInside(Each))
		{
			return false;
		}
	}
	return true;
}

template <bool bLoop>
typename TSegmentArray2D<bLoop>::FIntersection
TSegmentArray2D<bLoop>::FindClosestPointTo(const FVector2D& Point) const
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

template <bool bLoop>
TOptional<typename TSegmentArray2D<bLoop>::FIntersection>
TSegmentArray2D<bLoop>::FindIntersection(const UE::Geometry::FSegment2d& Segment) const
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

template <bool bLoop>
TArray<typename TSegmentArray2D<bLoop>::FIntersection>
TSegmentArray2D<bLoop>::FindAllIntersections(const UE::Geometry::FSegment2d& Segment) const
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

template <bool bLoop>
TOptional<FVector2D> TSegmentArray2D<bLoop>::Attach(const FVector2D& Point, const FVector2D& Direction) const
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

template <bool bLoop>
UE::Geometry::FSegment2d TSegmentArray2D<bLoop>::Clip(const UE::Geometry::FSegment2d& Segment) const requires bLoop
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

template <bool bLoop>
template <CSegmentArray2D SegmentArrayType>
auto TSegmentArray2D<bLoop>::Union(SegmentArrayType&& Path) requires bLoop
{
	TArray<FUnionResult> Ret;
	for (FSegmentArray2D& CleanPath : SplitIntoCleanPaths(Forward<SegmentArrayType>(Path)))
	{
		if (TOptional<FUnionResult> UnionResult = UnionAssumeTwoIntersections(MoveTemp(CleanPath)))
		{
			Ret.Add(MoveTemp(*UnionResult));
		}
	}
	return Ret;
}


template <bool bLoop>
template <CSegmentArray2D SegmentArrayType>
bool TSegmentArray2D<bLoop>::Difference(SegmentArrayType&& Path) requires bLoop
{
	TArray<FSegmentArray2D> CleanPaths = SplitIntoCleanPaths(Forward<SegmentArrayType>(Path));

	TArray<FLoopedSegmentArray2D> Islands;
	for (int32 i = 0; i < CleanPaths.Num(); i++)
	{
		Islands.Add(*this);
	}

	for (int32 i = 0; i < Islands.Num(); ++i)
	{
		for (int32 j = 0; j < CleanPaths.Num(); ++j)
		{
			Islands[i].DifferenceAssumeTwoIntersections(CleanPaths[(i + j) % CleanPaths.Num()]);
		}
	}

	if (FLoopedSegmentArray2D* LargestArea = Algo::MaxElementBy(Islands, &FLoopedSegmentArray2D::CalculateArea))
	{
		if (LargestArea->CalculateArea() < CalculateArea())
		{
			Points = MoveTemp(LargestArea->Points);
			return true;
		}
	}

	return false;
}


template <bool bLoop>
TArray<FSegmentArray2D> TSegmentArray2D<bLoop>::SplitIntoCleanPaths(FSegmentArray2D Path)
{
	struct FPathIntersection
	{
		int32 PathSegmentIndex;
		FVector2D Point;
	};

	// Segment의 양 지점이 Boundary 위에 정확하게 걸쳐져 있는 경우 Intersection Test에 실패할 수 있기 때문에 조금 늘려줌
	Path.SetPoint(0, Path.GetPoint(0) - Path[0].Direction * UE_KINDA_SMALL_NUMBER);
	Path.SetPoint(-1, Path.GetPoint(-1) + Path.GetSegmentDirection(-1) * UE_KINDA_SMALL_NUMBER);

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
		if (Left.PathSegmentIndex == Right.PathSegmentIndex)
		{
			const FVector2D SegmentStart = Path[Left.PathSegmentIndex].StartPoint();
			return (Left.Point - SegmentStart).Length() < (Right.Point - SegmentStart).Length();
		}

		return Left.PathSegmentIndex < Right.PathSegmentIndex;
	});

	if (AllIntersections.Num() < 2)
	{
		return {};
	}

	TArray<FSegmentArray2D> Ret;
	for (int32 i = 0; i < AllIntersections.Num() - 1; i++)
	{
		const FPathIntersection& Intersection0 = AllIntersections[i];
		const FPathIntersection& Intersection1 = AllIntersections[i + 1];

		FSegmentArray2D Sub = Path.SubArray(Intersection0.PathSegmentIndex, Intersection1.PathSegmentIndex);
		Sub.SetPoint(0, Intersection0.Point);
		Sub.SetPoint(-1, Intersection1.Point);
		Ret.Add(MoveTemp(Sub));
	}

	return Ret;
}

template <bool bLoop>
auto TSegmentArray2D<bLoop>::UnionAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop
{
	if (IsInside(Path[0].Center))
	{
		return TOptional<FUnionResult>{};
	}

	const FIntersection BoundarySrcSegment = FindClosestPointTo(Path.GetPoint(0));
	const FIntersection BoundaryDestSegment = FindClosestPointTo(Path.GetPoint(-1));

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

	const float Area0 = SrcToDestReplaced.CalculateArea();
	const float Area1 = DestToSrcReplaced.CalculateArea();
	const float CurrentArea = CalculateArea();

	// 모종의 이유로 맨 위의 조기 리턴 체크에서 걸러지지 않는 경우가 있음
	// TODO 원인에 대해서 조사해야 됨 (대략 path가 area의 경계와 완전히 겹치는 경우에 발생 가능)
	if (CurrentArea >= Area0 && CurrentArea >= Area1)
	{
		return TOptional<FUnionResult>{};
	}

	FSegmentArray2D& UsedPath = Area0 < Area1 ? ReversedPath : Path;
	Points = Area0 < Area1 ? MoveTemp(DestToSrcReplaced.Points) : MoveTemp(SrcToDestReplaced.Points);

	return TOptional<FUnionResult>{{.CorrectlyAlignedPath = MoveTemp(UsedPath)}};
}


template <bool bLoop>
void TSegmentArray2D<bLoop>::DifferenceAssumeTwoIntersections(FSegmentArray2D Path) requires bLoop
{
	if (!IsInside(Path[0].Center))
	{
		return;
	}

	const FIntersection BoundarySrcSegment = FindClosestPointTo(Path.GetPoint(0));
	const FIntersection BoundaryDestSegment = FindClosestPointTo(Path.GetPoint(-1));

	FLoopedSegmentArray2D SrcToDestReplaced = *this;
	SrcToDestReplaced.ReplacePointsBySegmentIndices(
		BoundarySrcSegment.SegmentIndex,
		BoundaryDestSegment.SegmentIndex,
		Path.GetPoints());

	Points = MoveTemp(SrcToDestReplaced.Points);
}
