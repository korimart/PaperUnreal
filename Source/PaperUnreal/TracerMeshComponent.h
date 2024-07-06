﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/SegmentArray.h"
#include "Components/DynamicMeshComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/Utils.h"
#include "TracerMeshComponent.generated.h"


class FDynamicMeshSegment2D
{
public:
	void SetDynamicMesh(UDynamicMesh* InDynamicMesh)
	{
		DynamicMesh = InDynamicMesh;
		Segments.Empty();
		DynamicMeshVertexIndices.Empty();
	}

	void AddVertex(const FVector2D& Position)
	{
		Segments.AddPoint(Position);
		// TODO 50 조절 가능하게
		DynamicMeshVertexIndices.Push(DynamicMesh->GetMeshRef().AppendVertex(FVector{Position, 51.f}));
	}

	void SetLastVertexPosition(const FVector2D& NewPosition)
	{
		if (!DynamicMeshVertexIndices.IsEmpty())
		{
			Segments.SetLastPoint(NewPosition);
			// TODO 50 조절 가능하게
			DynamicMesh->GetMeshRef().SetVertex(DynamicMeshVertexIndices.Last(), FVector{NewPosition, 51.f});
		}
	}

	const FSegmentArray2D& GetSegmentArray() const
	{
		return Segments;
	}

	const FVector2D& GetLastVertexPosition() const
	{
		return Segments.GetLastPoint();
	}

	int GetLastVertexDynamicIndex(int32 IndexFromLast = 0) const
	{
		return DynamicMeshVertexIndices[DynamicMeshVertexIndices.Num() - IndexFromLast - 1];
	}

private:
	FSegmentArray2D Segments;
	TWeakObjectPtr<UDynamicMesh> DynamicMesh;
	TArray<int> DynamicMeshVertexIndices;
};


UCLASS()
class UTracerMeshComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	int32 GetVertexCount() const
	{
		return DynamicMeshComponent->GetMesh()->VertexCount();
	}

	std::tuple<FVector2D, FVector2D> GetLastVertices() const
	{
		return {LeftSegments.GetLastVertexPosition(), RightSegments.GetLastVertexPosition()};
	}

	void Reset()
	{
		DynamicMeshComponent->GetDynamicMesh()->Reset();

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMeshComponent->GetMesh()->Attributes()->PrimaryNormals();
		NormalOverlay->AppendElement(FVector3f::UnitZ());
		NormalOverlay->AppendElement(FVector3f::UnitZ());
		NormalOverlay->AppendElement(FVector3f::UnitZ());

		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynamicMeshComponent->GetMesh()->Attributes()->PrimaryUV();
		UVOverlay->AppendElement({0.f, 0.f});
		UVOverlay->AppendElement({1.f, 0.f});
		UVOverlay->AppendElement({0.f, 1.f});
		UVOverlay->AppendElement({1.f, 1.f});

		LeftSegments.SetDynamicMesh(DynamicMeshComponent->GetDynamicMesh());
		RightSegments.SetDynamicMesh(DynamicMeshComponent->GetDynamicMesh());
	}

	void AppendVertices(const FVector2D& Left, const FVector2D& Right)
	{
		LeftSegments.AddVertex(Left);
		RightSegments.AddVertex(Right);

		if (LeftSegments.GetSegmentArray().PointCount() >= 2)
		{
			const int V0 = LeftSegments.GetLastVertexDynamicIndex(1);
			const int V1 = RightSegments.GetLastVertexDynamicIndex(1);
			const int V2 = LeftSegments.GetLastVertexDynamicIndex();
			const int V3 = RightSegments.GetLastVertexDynamicIndex();

			FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();

			const int Tri0 = Mesh->AppendTriangle(V0, V1, V2);
			const int Tri1 = Mesh->AppendTriangle(V1, V3, V2);

			Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri0, {0, 1, 2});
			Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri1, {0, 1, 2});
			Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri0, {0, 1, 2});
			Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri1, {1, 3, 2});
		}

		DynamicMeshComponent->NotifyMeshModified();
	}

	void SetLastVertices(const FVector2D& Left, const FVector2D& Right)
	{
		LeftSegments.SetLastVertexPosition(Left);
		RightSegments.SetLastVertexPosition(Right);
		DynamicMeshComponent->FastNotifyPositionsUpdated();
	}

	void ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
	{
		DynamicMeshComponent->ConfigureMaterialSet(NewMaterialSet);
	}

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FDynamicMeshSegment2D LeftSegments;
	FDynamicMeshSegment2D RightSegments;

	UTracerMeshComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->RegisterComponent();
		Reset();
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		DynamicMeshComponent->DestroyComponent();
		DynamicMeshComponent = nullptr;
	}
};


UCLASS()
class UTracerPathComponent : public UActorComponent2, public ITracerPathGenerator
{
	GENERATED_BODY()

public:
	// UTracerPathComponent의 구현은 옛날 Event를 기록하지 않음 (새 Event만 받을 수 있음)
	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() override
	{
		return CreateMulticastValueGenerator(this, TArray<FTracerPathEvent>{}, OnPathEvent);
	}

	const FSegmentArray2D& GetPath() const { return Path; }

	void SetNoPathArea(UAreaBoundaryComponent* Area)
	{
		NoPathArea = Area;
	}

private:
	UPROPERTY()
	UAreaBoundaryComponent* NoPathArea;

	bool bGeneratedThisFrame = false;
	FSegmentArray2D Path;

	UTracerPathComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(NoPathArea));
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bGeneratedLastFrame = bGeneratedThisFrame;
		const bool bWillGenerateThisFrame = !NoPathArea->IsInside(GetOwner()->GetActorLocation());

		bGeneratedThisFrame = false;

		if (!bGeneratedLastFrame && bWillGenerateThisFrame)
		{
			Path.Empty();
			OnPathEvent.Broadcast({.Event = ETracerPathEvent::GenerationStarted});
		}

		if (bWillGenerateThisFrame)
		{
			Generate();
			bGeneratedThisFrame = true;
		}

		if (bGeneratedLastFrame && !bGeneratedThisFrame)
		{
			Path.SetLastPoint(NoPathArea->FindClosestPointOnBoundary2D(Path.GetLastPoint()).GetPoint());
			OnPathEvent.Broadcast(CreateEventWithPointAndDir(ETracerPathEvent::LastPointModified));
			OnPathEvent.Broadcast({.Event = ETracerPathEvent::GenerationEnded});
		}
	}

	void Generate()
	{
		const FVector2D ActorLocation2D{GetOwner()->GetActorLocation()};

		if (Path.PointCount() == 0)
		{
			Path.AddPoint(ActorLocation2D);
			Path.SetLastPoint(NoPathArea->FindClosestPointOnBoundary2D(Path.GetLastPoint()).GetPoint());
			OnPathEvent.Broadcast(CreateEventWithPointAndDir(ETracerPathEvent::NewPointGenerated));
			return;
		}

		if (Path.GetLastPoint().Equals(ActorLocation2D))
		{
			return;
		}

		if (Path.PointCount() < 3)
		{
			Path.AddPoint(ActorLocation2D);
			OnPathEvent.Broadcast(CreateEventWithPointAndDir(ETracerPathEvent::NewPointGenerated));
			return;
		}

		const float Curvature = [&]()
		{
			const FVector2D& Position0 = Path.GetLastPoint(1);
			const FVector2D& Position1 = Path.GetLastPoint();
			const FVector2D& Position2 = ActorLocation2D;

			const float ASideLength = (Position1 - Position2).Length();
			const float BSideLength = (Position0 - Position2).Length();
			const float CSideLength = (Position0 - Position1).Length();

			const float TriangleArea = 0.5f * FMath::Abs(
				Position0.X * (Position1.Y - Position2.Y)
				+ Position1.X * (Position2.Y - Position0.Y)
				+ Position2.X * (Position0.Y - Position1.Y));

			return 4.f * TriangleArea / ASideLength / BSideLength / CSideLength;
		}();

		const float CurrentDeviation = [&]()
		{
			const FVector2D DeviatingVector = ActorLocation2D - Path.GetLastPoint(1);
			const FVector2D StraightVector = Path.GetLastSegmentDirection(1);
			const FVector2D Proj = StraightVector * DeviatingVector.Dot(StraightVector);
			return (DeviatingVector - Proj).Length();
		}();

		if (Curvature > 0.005f || CurrentDeviation > 10.f)
		{
			Path.AddPoint(ActorLocation2D);
			OnPathEvent.Broadcast(CreateEventWithPointAndDir(ETracerPathEvent::NewPointGenerated));
		}
		else
		{
			Path.SetLastPoint(ActorLocation2D);
			OnPathEvent.Broadcast(CreateEventWithPointAndDir(ETracerPathEvent::LastPointModified));
		}
	}

	FTracerPathEvent CreateEventWithPointAndDir(ETracerPathEvent Event) const
	{
		return FTracerPathEvent
		{
			.Event = Event,
			.Point = Path.GetLastPoint(),
			.PathDirection = FVector2D{GetOwner()->GetActorForwardVector()},
		};
	}
};
