// Fill out your copyright notice in the Description page of Project Settings.

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
		// TODO 50 조절 가능하게
		DynamicMeshVertexIndices.Push(DynamicMesh->GetMeshRef().AppendVertex(FVector{Position, 50.f}));
	}

	void SetLastVertexPosition(const FVector2D& NewPosition)
	{
		if (!DynamicMeshVertexIndices.IsEmpty())
		{
			Segments.SetLastPoint(NewPosition);
			// TODO 50 조절 가능하게
			DynamicMesh->GetMeshRef().SetVertex(DynamicMeshVertexIndices.Last(), FVector{NewPosition, 50.f});
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


class FTracingMeshEditor
{
public:
	static std::tuple<FVector2D, FVector2D, FVector2D> CreateVertexPositions(const AActor* ActorToTrace)
	{
		const FVector2D ActorLocation2D{ActorToTrace->GetActorLocation()};
		const FVector2D ActorRight2D = FVector2D{ActorToTrace->GetActorRightVector()}.GetSafeNormal();
		return {ActorLocation2D - 50.f * ActorRight2D, ActorLocation2D, ActorLocation2D + 50.f * ActorRight2D};
	}
	
	const FSegmentArray2D& GetLeftSegmentArray2D() const
	{
		return LeftSegments.GetSegmentArray();
	}

	const FSegmentArray2D& GetCenterSegmentArray2D() const
	{
		return CenterSegments;
	}
	
	const FSegmentArray2D& GetRightSegmentArray2D() const
	{
		return RightSegments.GetSegmentArray();
	}

	void SetDynamicMeshComponent(UDynamicMeshComponent* Component)
	{
		TargetComponent = Component;
		Reset();
	}

	void Reset()
	{
		TargetComponent->GetDynamicMesh()->Reset();

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
		LeftSegments.SetDynamicMesh(TargetComponent->GetDynamicMesh());
		RightSegments.SetDynamicMesh(TargetComponent->GetDynamicMesh());
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

	template <typename FuncType>
	void ModifyLastVertexPositions(FuncType&& Func)
	{
		FVector2D NewLeft = LeftSegments.GetLastVertexPosition();
		FVector2D NewCenter = CenterSegments.GetLastPoint();
		FVector2D NewRight = RightSegments.GetLastVertexPosition();

		Func(NewLeft, NewCenter, NewRight);

		LeftSegments.SetLastVertexPosition(NewLeft);
		CenterSegments.SetLastPoint(NewCenter);
		RightSegments.SetLastVertexPosition(NewRight);

		TargetComponent->FastNotifyPositionsUpdated();
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
			ModifyLastVertexPositions([&](auto& Left, auto& Center, auto& Right)
			{
				std::tie(Left, Center, Right) = CreateVertexPositions(ActorToTrace);
			});
		}
	}
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UTracingMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_ThreeParams(FEdgeModifier, FVector2D&, FVector2D&, FVector2D&);
	FEdgeModifier FirstEdgeModifier;
	FEdgeModifier LastEdgeModifier;

	void SetTracingEnabled(bool bEnable)
	{
		if (bEnable)
		{
			TracingMeshEditor.Trace(GetOwner());
			TracingMeshEditor.ModifyLastVertexPositions([this](auto&... Vertices)
			{
				FirstEdgeModifier.ExecuteIfBound(Vertices...);
			});
		}
		else
		{
			TracingMeshEditor.ModifyLastVertexPositions([this](auto&... Vertices)
			{
				LastEdgeModifier.ExecuteIfBound(Vertices...);
			});
		}

		SetComponentTickEnabled(bEnable);
	}

	bool IsTracing() const
	{
		return IsComponentTickEnabled();
	}

	void Reset()
	{
		TracingMeshEditor.Reset();
	}
	
	const FSegmentArray2D& GetLeftSegmentArray2D() const
	{
		return TracingMeshEditor.GetLeftSegmentArray2D();
	}

	const FSegmentArray2D& GetCenterSegmentArray2D() const
	{
		return TracingMeshEditor.GetCenterSegmentArray2D();
	}
	
	const FSegmentArray2D& GetRightSegmentArray2D() const
	{
		return TracingMeshEditor.GetRightSegmentArray2D();
	}

private:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UMaterialInstance> TraceMaterial;

	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	FTracingMeshEditor TracingMeshEditor;

	UTracingMeshComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		PrimaryComponentTick.bStartWithTickEnabled = false;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->RegisterComponent();

		TracingMeshEditor.SetDynamicMeshComponent(DynamicMeshComponent);

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
