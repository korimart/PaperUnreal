// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "AreaMeshComponent.generated.h"


/**
 * counter-clockwise
 */
class FPolygonBoundary2D
{
public:
	struct FIntersection
	{
		int32 SegmentStartIndex;
		FVector2D ExactPosition;
	};

	bool IsValid() const
	{
		return Points.Num() >= 3;
	}

	bool IsInside(const FVector2D& Point) const
	{
		if (!IsValid())
		{
			return false;
		}

		const auto IsPointToTheLeftOfLine = [](
			const FVector2D& PointOnLine0,
			const FVector2D& PointOnLine1,
			const FVector2D& Point)
		{
			const FVector2D Line = PointOnLine1 - PointOnLine0;
			const FVector2D ToPoint = Point - PointOnLine0;
			const float CrossProduct = FVector2D::CrossProduct(Line, ToPoint);
			return CrossProduct < 0.f;
		};

		return AreAllOfSegments([&](const FVector2D& Start, const FVector2D& End)
		{
			return IsPointToTheLeftOfLine(Start, End, Point);
		});
	}

	TOptional<FIntersection> DoesSegmentIntersect(const FVector2D& TargetSegmentStart, const FVector2D& TargetSegmentEnd) const
	{
		if (!IsValid())
		{
			return {};
		}

		FVector Intersection;
		int32 Index = -1;
		if (IsAnyOfSegments([&](const FVector2D& EachStart, const FVector2D& EachEnd)
		{
			Index++;
			return FMath::SegmentIntersection2D(
				FVector{TargetSegmentStart, 0.f},
				FVector{TargetSegmentEnd, 0.f},
				FVector{EachStart, 0.f},
				FVector{EachEnd, 0.f},
				Intersection);
		}))
		{
			FIntersection Ret;
			Ret.ExactPosition.X = Intersection.X;
			Ret.ExactPosition.Y = Intersection.Y;
			Ret.SegmentStartIndex = Index;
			return Ret;
		}

		return {};
	}

	void Replace(int32 FirstIndex, int32 LastIndex, const TArray<FVector2D>& NewPoints)
	{
		const int32 Count = FMath::Min(LastIndex - FirstIndex + 1, Points.Num());
		Points.RemoveAt(FirstIndex, Count);
		Points.Insert(NewPoints, FirstIndex);
	}

	void SetSelfInDynamicMesh(UDynamicMesh* DynamicMesh) const
	{
		DynamicMesh->Reset();
		
		if (!IsValid())
		{
			return;
		}

		TArray<int32> PositionsToVertexIds;
		bool bHasDuplicateVertices;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDelaunayTriangulation2D(
			DynamicMesh,
			{},
			FTransform{FVector{0.f, 0.f, 100.f}},
			Points,
			{},
			{},
			PositionsToVertexIds,
			bHasDuplicateVertices);
	}

private:
	TArray<FVector2D> Points;

	template <typename FuncType>
	TOptional<int32> ForEachSegment(FuncType&& Func) const
	{
		for (int32 i = 0; i < Points.Num() - 1; i++)
		{
			if (!Func(Points[i], Points[i + 1]))
			{
				return i;
			}
		}

		if (!Func(Points.Last(), Points[0]))
		{
			return Points.Num() - 1;
		}

		return {};
	}

	template <typename FuncType>
	bool AreAllOfSegments(FuncType&& Func) const
	{
		return !ForEachSegment([&](const FVector2D& Start, const FVector2D& End)
		{
			return Func(Start, End);
		});
	}

	template <typename FuncType>
	bool IsAnyOfSegments(FuncType&& Func) const
	{
		return !!ForEachSegment([&](const FVector2D& Start, const FVector2D& End)
		{
			return !Func(Start, End);
		});
	}
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAreaMeshComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FPolygonBoundary2D AreaBoundary;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
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
			const FTransform Transform{FQuat{}, GetOwner()->GetActorLocation(), FVector{100.f}};
			
			TArray<FVector2D> Ret;
			for (int32 AngleStep = 0; AngleStep < 16; AngleStep++)
			{
				const float ThisAngle = 2.f * UE_PI / 16.f * AngleStep;
				const FVector Vertex = Transform.TransformPosition(FVector{FMath::Cos(ThisAngle), FMath::Sin(ThisAngle), 0.f});
				Ret.Add(FVector2D{Vertex});
			}
			return Ret;
		}();

		AreaBoundary.Replace(0, 0, VertexPositions);
		AreaBoundary.SetSelfInDynamicMesh(DynamicMeshComponent->GetDynamicMesh());
	}
};
