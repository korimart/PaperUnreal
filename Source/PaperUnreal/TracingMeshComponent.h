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
		RebuildMesh();
	}

	void AddTracingPoint(const AActor* ActorToTrace)
	{
		TracePoints.Emplace(ActorToTrace->GetActorLocation(), ActorToTrace->GetActorRightVector());
		RebuildMesh();
	}

private:
	TWeakObjectPtr<UDynamicMeshComponent> TargetComponent;

	struct FTracePoint
	{
		FVector Location;
		FVector RightVector;
	};

	TArray<FTracePoint> TracePoints;

	void RebuildMesh()
	{
		if (!TargetComponent.IsValid())
		{
			return;
		}

		TargetComponent->GetDynamicMesh()->Reset();

		TArray<int> VertexIds;
		for (const auto& [EachLocation, EachRightVector] : TracePoints)
		{
			VertexIds.Add(TargetComponent->GetMesh()->AppendVertex(EachLocation - 50.f * EachRightVector));
			VertexIds.Add(TargetComponent->GetMesh()->AppendVertex(EachLocation + 50.f * EachRightVector));
		}
		
		for (int32 i = 3; i < VertexIds.Num(); i = i + 2)
		{
			const int V0 = VertexIds[i - 3];
			const int V1 = VertexIds[i - 2];
			const int V2 = VertexIds[i - 1];
			const int V3 = VertexIds[i];

			TargetComponent->GetMesh()->AppendTriangle(V0, V1, V2);
			TargetComponent->GetMesh()->AppendTriangle(V1, V3, V2);
		}

		TargetComponent->NotifyMeshModified();
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
		PrimaryComponentTick.TickInterval = 1.f / 30.f;
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

		TracingMeshEditor.AddTracingPoint(GetOwner());
	}
};
