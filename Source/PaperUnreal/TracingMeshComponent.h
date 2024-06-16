﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentArray.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/AssetManager.h"
#include "TracingMeshComponent.generated.h"


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
		DynamicMeshVertexIndices.Push(DynamicMesh->GetMeshRef().AppendVertex(FVector{Position, 50.f}));
	}

	void SetLastVertexPosition(const FVector2D& NewPosition)
	{
		if (!DynamicMeshVertexIndices.IsEmpty())
		{
			Segments.SetLastPoint(NewPosition);
			DynamicMesh->GetMeshRef().SetVertex(DynamicMeshVertexIndices.Last(), FVector{NewPosition, 50.f});
		}
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


class FTracingMeshEditor
{
public:
	void SetTargetMeshComponent(UDynamicMeshComponent* InTarget)
	{
		TargetComponent = InTarget;
		TargetComponent->GetDynamicMesh()->Reset();
		TargetComponent->GetMesh()->EnableAttributes();

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = TargetComponent->GetMesh()->Attributes()->PrimaryNormals();
		NormalOverlay->AppendElement(FVector3f::UnitZ());
		NormalOverlay->AppendElement(FVector3f::UnitZ());
		NormalOverlay->AppendElement(FVector3f::UnitZ());

		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = TargetComponent->GetMesh()->Attributes()->PrimaryUV();
		UVOverlay->AppendElement({0.f, 0.f});
		UVOverlay->AppendElement({1.f, 0.f});
		UVOverlay->AppendElement({0.f, 1.f});
		UVOverlay->AppendElement({1.f, 1.f});

		CenterSegments.Empty();
		LeftSegments.SetDynamicMesh(InTarget->GetDynamicMesh());
		RightSegments.SetDynamicMesh(InTarget->GetDynamicMesh());
	}

	void Trace(const AActor* ActorToTrace)
	{
		if (!TargetComponent.IsValid())
		{
			return;
		}

		if (CenterSegments.PointCount() == 0)
		{
			ExtrudeMesh(ActorToTrace);
			return;
		}

		const FVector2D ActorLocation2D{ActorToTrace->GetActorLocation()};

		if (CenterSegments.GetLastPoint().Equals(ActorLocation2D))
		{
			return;
		}

		if (CenterSegments.PointCount() < 3)
		{
			ExtrudeMesh(ActorToTrace);
			return;
		}

		const float Curvature = [&]()
		{
			const FVector2D& Position0 = CenterSegments.GetLastPoint(1);
			const FVector2D& Position1 = CenterSegments.GetLastPoint();
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
			const FVector2D DeviatingVector = ActorLocation2D - CenterSegments.GetLastPoint(1);
			const FVector2D StraightVector = CenterSegments.GetLastSegmentDirection(1);
			const FVector2D Proj = StraightVector * DeviatingVector.Dot(StraightVector);
			return (DeviatingVector - Proj).Length();
		}();

		if (Curvature > 0.005f || CurrentDeviation > 10.f)
		{
			ExtrudeMesh(ActorToTrace);
		}
		else
		{
			ElongateMesh(ActorToTrace);
		}
	}

private:
	TWeakObjectPtr<UDynamicMeshComponent> TargetComponent;
	FDynamicMeshSegment2D LeftSegments;
	FSegmentArray2D CenterSegments;
	FDynamicMeshSegment2D RightSegments;

	void ExtrudeMesh(const AActor* ActorToTrace)
	{
		const auto [Left, Center, Right] = CreateVertexPositions(ActorToTrace);
		LeftSegments.AddVertex(Left);
		CenterSegments.AddPoint(Center);
		RightSegments.AddVertex(Right);

		if (CenterSegments.PointCount() >= 2)
		{
			const int V0 = LeftSegments.GetLastVertexDynamicIndex(1);
			const int V1 = RightSegments.GetLastVertexDynamicIndex(1);
			const int V2 = LeftSegments.GetLastVertexDynamicIndex();
			const int V3 = RightSegments.GetLastVertexDynamicIndex();

			FDynamicMesh3* Mesh = TargetComponent->GetMesh();

			const int Tri0 = Mesh->AppendTriangle(V0, V1, V2);
			const int Tri1 = Mesh->AppendTriangle(V1, V3, V2);

			Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri0, {0, 1, 2});
			Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri1, {0, 1, 2});
			Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri0, {0, 1, 2});
			Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri1, {1, 3, 2});
		}

		TargetComponent->NotifyMeshModified();
	}

	void ElongateMesh(const AActor* ActorToTrace)
	{
		if (CenterSegments.PointCount() > 0)
		{
			const auto [Left, Center, Right] = CreateVertexPositions(ActorToTrace);
			LeftSegments.SetLastVertexPosition(Left);
			CenterSegments.SetLastPoint(Center);
			RightSegments.SetLastVertexPosition(Right);
			TargetComponent->FastNotifyPositionsUpdated();
		}
	}

	static std::tuple<FVector2D, FVector2D, FVector2D> CreateVertexPositions(const AActor* ActorToTrace)
	{
		const FVector2D ActorLocation2D{ActorToTrace->GetActorLocation()};
		const FVector2D ActorRight2D = FVector2D{ActorToTrace->GetActorRightVector()}.GetSafeNormal();
		return {ActorLocation2D - 50.f * ActorRight2D, ActorLocation2D, ActorLocation2D + 50.f * ActorRight2D};
	}
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UTracingMeshComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UMaterialInstance> TraceMaterial;

	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FTracingMeshEditor TracingMeshEditor;

	UTracingMeshComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->RegisterComponent();

		TracingMeshEditor.SetTargetMeshComponent(DynamicMeshComponent);

		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			TraceMaterial.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				DynamicMeshComponent->ConfigureMaterialSet({TraceMaterial.Get()});
			}));
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		DynamicMeshComponent->DestroyComponent();
		DynamicMeshComponent = nullptr;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		TracingMeshEditor.Trace(GetOwner());
	}
};
