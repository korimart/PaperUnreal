// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "AreaMeshComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAreaMeshComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UDynamicMeshComponent* DynamicMeshComponent;

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
		const TArray<FVector2d> VertexPositions = [&]()
		{
			TArray<FVector2d> Ret;

			for (int32 AngleStep = 0; AngleStep < 16; AngleStep++)
			{
				const float ThisAngle = 2.f * UE_PI / 16.f * AngleStep;
				Ret.Add({FMath::Cos(ThisAngle), FMath::Sin(ThisAngle)});
			}

			return Ret;
		}();

		const FTransform Transform
		{
			FQuat{}, GetOwner()->GetActorLocation(), FVector{100.f}
		};

		TArray<int32> PositionsToVertexIds;
		bool bHasDuplicateVertices;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDelaunayTriangulation2D(
			DynamicMeshComponent->GetDynamicMesh(),
			{},
			Transform,
			VertexPositions,
			{},
			{},
			PositionsToVertexIds,
			bHasDuplicateVertices);
	}
};
