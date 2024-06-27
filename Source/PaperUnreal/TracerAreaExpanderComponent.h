// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Components/ActorComponent.h"
#include "TracerAreaExpanderComponent.generated.h"


UCLASS()
class UTracerAreaExpanderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void SetExpansionTarget(UAreaMeshComponent* Target)
	{
		AreaMeshComponent = Target;
	}

private:
	UPROPERTY()
	UAreaMeshComponent* AreaMeshComponent;
	
	UPROPERTY()
	UTracerMeshComponent* TracerMeshComponent;

	UPROPERTY()
	UTracerVertexGeneratorComponent* VertexGeneratorComponent;

	UTracerAreaExpanderComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		check(GetOwner()->IsA<APawn>());

		TracerMeshComponent = GetOwner()->FindComponentByClass<UTracerMeshComponent>();
		check(IsValid(TracerMeshComponent));

		VertexGeneratorComponent = NewObject<UTracerVertexGeneratorComponent>(GetOwner());
		VertexGeneratorComponent->SetGenerationDestination(TracerMeshComponent);
		VertexGeneratorComponent->FirstEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
		VertexGeneratorComponent->LastEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
		VertexGeneratorComponent->RegisterComponent();
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bOwnerIsInsideArea = AreaMeshComponent->IsInside(GetOwner()->GetActorLocation());

		if (VertexGeneratorComponent->IsGenerating() && bOwnerIsInsideArea)
		{
			VertexGeneratorComponent->SetGenerationEnabled(false);
			AreaMeshComponent->ExpandByPath(TracerMeshComponent->GetCenterSegmentArray2D());
		}
		else if (!VertexGeneratorComponent->IsGenerating() && !bOwnerIsInsideArea)
		{
			TracerMeshComponent->Reset();
			VertexGeneratorComponent->SetGenerationEnabled(true);
		}
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);
		
		VertexGeneratorComponent->DestroyComponent();
		VertexGeneratorComponent = nullptr;
	}

	void AttachVertexToAreaBoundary(FVector2D& Vertex) const
	{
		Vertex = AreaMeshComponent->FindClosestPointOnBoundary2D(Vertex).GetPoint();
	}
};
