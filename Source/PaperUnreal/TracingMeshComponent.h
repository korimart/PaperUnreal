// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/AssetManager.h"
#include "TracingMeshComponent.generated.h"


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
		
		LastVertexId0.Reset();
		LastVertexId1.Reset();
	}

	void Trace(const AActor* ActorToTrace)
	{
		const FTracePoint TracePoint{ActorToTrace};

		if (!LastTracePoint && !LastLastTracePoint)
		{
			ExtrudeMesh(TracePoint);
			return;
		}

		if (LastTracePoint && *LastTracePoint == TracePoint)
		{
			return;
		}

		const FVector Direction0 = LastLastTracePoint
			? LastTracePoint->Location - LastLastTracePoint->Location : LastTracePoint->ForwardVector;
		const FVector Direction1 = TracePoint.Location - LastTracePoint->Location;
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(Direction0.CosineAngle2D(Direction1)));
		UE_LOG(LogTemp, Warning, TEXT("%f"), Angle);

		if (Angle < 10.f)
		{
			ElongateMesh(TracePoint);
		}
		else
		{
			ExtrudeMesh(TracePoint);
		}
	}

private:
	TWeakObjectPtr<UDynamicMeshComponent> TargetComponent;

	struct FTracePoint
	{
		FVector Location;
		FVector ForwardVector;
		FVector RightVector;

		FTracePoint(const AActor* Actor)
			: Location(Actor->GetActorLocation())
		{
			ForwardVector = Actor->GetActorForwardVector();
			ForwardVector.Z = 0.f;
			ForwardVector.Normalize();
			
			RightVector = FVector::UnitZ().Cross(ForwardVector);
		}

		TTuple<FVector, FVector> MakeVertexPositions() const
		{
			return MakeTuple(Location - 50.f * RightVector, Location + 50.f * RightVector);
		}

		friend bool operator==(const FTracePoint& Left, const FTracePoint& Right)
		{
			return Left.Location == Right.Location;
		}
	};

	TOptional<int> LastVertexId0;
	TOptional<int> LastVertexId1;

	TOptional<FTracePoint> LastLastTracePoint;
	TOptional<FTracePoint> LastTracePoint;

	void ExtrudeMesh(const FTracePoint& TracePoint)
	{
		if (TargetComponent.IsValid())
		{
			FDynamicMesh3* Mesh = TargetComponent->GetMesh();

			const TOptional<int> LastLastVertexId0 = LastVertexId0;
			const TOptional<int> LastLastVertexId1 = LastVertexId1;

			const auto& [LeftVertexPosition, RightVertexPosition] = TracePoint.MakeVertexPositions();
			LastVertexId0 = Mesh->AppendVertex(LeftVertexPosition);
			LastVertexId1 = Mesh->AppendVertex(RightVertexPosition);

			if (LastLastVertexId0 && LastLastVertexId1)
			{
				const int V0 = *LastLastVertexId0;
				const int V1 = *LastLastVertexId1;
				const int V2 = *LastVertexId0;
				const int V3 = *LastVertexId1;

				const int Tri0 = Mesh->AppendTriangle(V0, V1, V2);
				const int Tri1 = Mesh->AppendTriangle(V1, V3, V2);

				Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri0, {0, 1, 2});
				Mesh->Attributes()->PrimaryNormals()->SetTriangle(Tri1, {0, 1, 2});
				Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri0, {0, 1, 2});
				Mesh->Attributes()->PrimaryUV()->SetTriangle(Tri1, {1, 3, 2});
			}

			TargetComponent->NotifyMeshModified();
			
			LastLastTracePoint = LastTracePoint;
			LastTracePoint = TracePoint;
		}
	}

	void ElongateMesh(const FTracePoint& TracePoint)
	{
		if (TargetComponent.IsValid() && LastVertexId0 && LastVertexId1)
		{
			const auto& [LeftVertexPosition, RightVertexPosition] = TracePoint.MakeVertexPositions();
			TargetComponent->GetMesh()->SetVertex(*LastVertexId0, LeftVertexPosition);
			TargetComponent->GetMesh()->SetVertex(*LastVertexId1, RightVertexPosition);
			TargetComponent->FastNotifyPositionsUpdated();
		}
	}
};


UCLASS(meta=(BlueprintSpawnableComponent))
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
		DynamicMeshComponent->SetIsReplicated(false);
		DynamicMeshComponent->RegisterComponent();
		
		TracingMeshEditor.SetTargetMeshComponent(DynamicMeshComponent);

		UAssetManager::GetStreamableManager().RequestAsyncLoad(TraceMaterial.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			DynamicMeshComponent->ConfigureMaterialSet({ TraceMaterial.Get() });
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
