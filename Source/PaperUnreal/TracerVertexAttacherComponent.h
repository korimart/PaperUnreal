// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/Utils.h"
#include "TracerVertexAttacherComponent.generated.h"


UCLASS()
class UTracerVertexAttacherComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void SetVertexGenerator(UTracerVertexGeneratorComponent* Generator)
	{
		VertexGeneratorComponent = Generator;
	}
	
	void SetAttachDestination(UAreaMeshComponent* Dest)
	{
		AttachDestination = Dest;
	}

private:
	UPROPERTY()
	UTracerVertexGeneratorComponent* VertexGeneratorComponent;
	
	UPROPERTY()
	UAreaMeshComponent* AttachDestination;

	UTracerVertexAttacherComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();
		
		check(AllValid(VertexGeneratorComponent, AttachDestination));
		
		VertexGeneratorComponent->FirstEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
		VertexGeneratorComponent->LastEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
	}
	
	void AttachVertexToAreaBoundary(FVector2D& Vertex) const
	{
		Vertex = AttachDestination->FindClosestPointOnBoundary2D(Vertex).GetPoint();
	}
};