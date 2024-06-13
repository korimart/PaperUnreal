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
		const FTransform Transform{ GetOwner()->GetActorLocation() };
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(DynamicMeshComponent->GetDynamicMesh(), {}, Transform);
	}
};
