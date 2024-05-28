// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "TracingMeshComponent.generated.h"


UCLASS(meta=(BlueprintSpawnableComponent))
class UTracingMeshComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

	TArray<FTransform> Path;

	UTracingMeshComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
		PrimaryComponentTick.TickInterval = 1.f / 30.f;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() != NM_DedicatedServer)
		{
			DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetOwner(), TEXT("DynamicMeshComponent"));
			DynamicMeshComponent->SetIsReplicated(false);
			DynamicMeshComponent->RegisterComponent();
		}
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		if (IsValid(DynamicMeshComponent))
		{
			DynamicMeshComponent->DestroyComponent();
			DynamicMeshComponent = nullptr;
		}
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		Path.Add(GetOwner()->GetActorTransform());

		const FGeometryScriptPrimitiveOptions PrimitiveOptions
		{
			.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::SingleGroup,
		};

		const TArray<FVector2D> PolygonVertices
		{
			{-100.f, 100.f,},
			{-100.f, -100.f,},
			{100.f, -100.f,},
			{100.f, 100.f,},
		};

		DynamicMeshComponent->GetDynamicMesh()->Reset();
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
			DynamicMeshComponent->GetDynamicMesh(), PrimitiveOptions, {}, PolygonVertices, Path, false, true);
	}
};
