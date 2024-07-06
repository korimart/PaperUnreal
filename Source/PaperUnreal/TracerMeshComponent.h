// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Components/DynamicMeshComponent.h"
#include "Core/ActorComponent2.h"
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