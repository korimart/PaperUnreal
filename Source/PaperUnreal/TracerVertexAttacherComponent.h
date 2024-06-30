// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "TracerVertexAttacherComponent.generated.h"


UCLASS()
class UTracerVertexAttacherComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void SetVertexGenerator(UTracerVertexGeneratorComponent* Generator)
	{
		if (IsValid(VertexGeneratorComponent))
		{
			VertexGeneratorComponent->FirstEdgeModifier.Unbind();
			VertexGeneratorComponent->LastEdgeModifier.Unbind();
		}

		VertexGeneratorComponent = Generator;
		
		VertexGeneratorComponent->FirstEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
		VertexGeneratorComponent->LastEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
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
	
	void AttachVertexToAreaBoundary(FVector2D& Vertex) const
	{
		if (IsValid(AttachDestination))
		{
			Vertex = AttachDestination->FindClosestPointOnBoundary2D(Vertex).GetPoint();
		}
	}
};