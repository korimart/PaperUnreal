// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "TracingMeshComponent.generated.h"


class FTracingMeshEditor
{
public:
	void SetTargetMeshComponent(UDynamicMeshComponent* InTarget)
	{
		TargetComponent = InTarget;
		TargetComponent->GetDynamicMesh()->Reset();
		LastVertexId0 = {};
		LastVertexId1 = {};
	}

	void Trace(const AActor* ActorToTrace)
	{
		static int32 Counter = 0;
		Counter++;
		if (Counter % 120 == 0)
		{
			ExtrudeMesh({ActorToTrace});
		}
		else
		{
			ElongateMesh({ActorToTrace});
		}
	}

private:
	TWeakObjectPtr<UDynamicMeshComponent> TargetComponent;

	struct FTracePoint
	{
		FVector Location;
		FVector RightVector;

		FTracePoint(const AActor* Actor)
			: Location(Actor->GetActorLocation()), RightVector(Actor->GetActorRightVector())
		{
		}

		TTuple<FVector, FVector> MakeVertexPositions() const
		{
			return MakeTuple(Location - 50.f * RightVector, Location + 50.f * RightVector);
		}
	};

	TOptional<int> LastVertexId0;
	TOptional<int> LastVertexId1;

	void ExtrudeMesh(const FTracePoint& TracePoint)
	{
		if (TargetComponent.IsValid())
		{
			const TOptional<int> LastLastVertexId0 = LastVertexId0;
			const TOptional<int> LastLastVertexId1 = LastVertexId1;

			const auto& [LeftVertexPosition, RightVertexPosition] = TracePoint.MakeVertexPositions();
			LastVertexId0 = TargetComponent->GetMesh()->AppendVertex(LeftVertexPosition);
			LastVertexId1 = TargetComponent->GetMesh()->AppendVertex(RightVertexPosition);

			if (LastLastVertexId0 && LastLastVertexId1)
			{
				const int V0 = *LastLastVertexId0;
				const int V1 = *LastLastVertexId1;
				const int V2 = *LastVertexId0;
				const int V3 = *LastVertexId1;

				TargetComponent->GetMesh()->AppendTriangle(V0, V1, V2);
				TargetComponent->GetMesh()->AppendTriangle(V1, V3, V2);
			}

			TargetComponent->NotifyMeshModified();
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
