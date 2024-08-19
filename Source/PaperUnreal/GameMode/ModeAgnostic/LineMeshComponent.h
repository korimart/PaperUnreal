// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "PaperUnreal/AreaTracer/TracerPointEventListener.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "LineMeshComponent.generated.h"


UCLASS()
class ULineMeshComponent : public UActorComponent2, public ITracerPointEventListener
{
	GENERATED_BODY()

public:
	virtual void OnTracerBegin() override
	{
		Reset();
	}

	virtual void AddPoint(const FVector2D& Point) override
	{
		Points.Add(Point);

		if (Points.Num() == 1)
		{
			return;
		}

		FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();

		if (Points.Num() == 2)
		{
			const FVector2D ForwardDirection = (Points[1] - Points[0]).GetSafeNormal();
			const FVector2D RightDirection{-ForwardDirection.Y, ForwardDirection.X};
			LeftVertices.Add(Mesh->AppendVertex(FVector{Points[0] - RightDirection * HalfWidth, MeshHeight}));
			RightVertices.Add(Mesh->AppendVertex(FVector{Points[0] + RightDirection * HalfWidth, MeshHeight}));
		}

		const FVector2D ForwardDirection = (Points.Last() - Points.Last(1)).GetSafeNormal();
		const FVector2D RightDirection{-ForwardDirection.Y, ForwardDirection.X};

		LeftVertices.Add(Mesh->AppendVertex(FVector{Points.Last() - RightDirection * HalfWidth, MeshHeight}));
		RightVertices.Add(Mesh->AppendVertex(FVector{Points.Last() + RightDirection * HalfWidth, MeshHeight}));

		const int V0 = LeftVertices.Last(1);
		const int V1 = RightVertices.Last(1);
		const int V2 = LeftVertices.Last();
		const int V3 = RightVertices.Last();

		const int Tri0 = Mesh->AppendTriangle(V0, V1, V2);
		const int Tri1 = Mesh->AppendTriangle(V1, V3, V2);

		Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri0, {0, 1, 2});
		Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri1, {0, 1, 2});
		Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri0, {0, 1, 2});
		Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri1, {1, 3, 2});

		DynamicMeshComponent->NotifyMeshModified();
	}

	virtual void SetPoint(int32 Index, const FVector2D& Point) override
	{
		Index = Index < 0 ? Index + Points.Num() : Index;

		// 시간 관계 상 일단 Last 포지션만 지원함
		check(Index == Points.Num() - 1);

		Points[Index] = Point;

		if (Points.Num() < 2)
		{
			return;
		}

		const FVector2D ForwardDirection = (Points.Last() - Points.Last(1)).GetSafeNormal();
		const FVector2D RightDirection{-ForwardDirection.Y, ForwardDirection.X};

		FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
		Mesh->SetVertex(LeftVertices[Index], FVector{Points[Index] - RightDirection * HalfWidth, MeshHeight});
		Mesh->SetVertex(RightVertices[Index], FVector{Points[Index] + RightDirection * HalfWidth, MeshHeight});

		if (Points.Num() >= 4)
		{
			const FVector2D LastForwardDirection = (Points.Last(1) - Points.Last(2)).GetSafeNormal();
			const FVector2D PerpDirection = (ForwardDirection + LastForwardDirection).GetSafeNormal();
			const FVector2D NewRightDirection{-PerpDirection.Y, PerpDirection.X};

			const float Dot = ForwardDirection.Dot(-LastForwardDirection);
			const float Alpha = FMath::Abs(FMath::Acos(Dot));
			const float Length = HalfWidth / FMath::Sin(Alpha / 2.f);

			const FVector2D NewLeft{Points.Last(1) - NewRightDirection * Length};
			const FVector2D NewRight{Points.Last(1) + NewRightDirection * Length};

			Mesh->SetVertex(LeftVertices.Last(1), FVector{NewLeft, MeshHeight});
			Mesh->SetVertex(RightVertices.Last(1), FVector{NewRight, MeshHeight});
		}

		DynamicMeshComponent->FastNotifyPositionsUpdated();
	}

	void Reset()
	{
		Points.Empty();
		LeftVertices.Empty();
		RightVertices.Empty();
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
	}
	
	void ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
	{
		DynamicMeshComponent->ConfigureMaterialSet(NewMaterialSet);
		DynamicMeshComponent->NotifyMeshModified();
	}

private:
	static constexpr float HalfWidth = 30.f;
	static constexpr float MeshHeight = 0.1f;

	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	TArray<FVector2D> Points;
	TArray<int32> LeftVertices;
	TArray<int32> RightVertices;

	ULineMeshComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->RegisterComponent();
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		DynamicMeshComponent->DestroyComponent();
	}
};
