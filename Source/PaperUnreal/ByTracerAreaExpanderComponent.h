﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "EngineUtils.h"
#include "TracingMeshComponent.h"
#include "Components/ActorComponent.h"
#include "ByTracerAreaExpanderComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UByTracerAreaExpanderComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UAreaMeshComponent* AreaMeshComponent;

	UPROPERTY()
	UTracingMeshComponent* TracingMeshComponent;

	UByTracerAreaExpanderComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		check(GetOwner()->IsA<APawn>());

		// TODO replace this with team based search and more efficient one
		AreaMeshComponent = [&]() -> UAreaMeshComponent* {
			for (FActorIterator It{GetWorld()}; It; ++It)
			{
				if (const auto Found = It->FindComponentByClass<UAreaMeshComponent>())
				{
					return Found;
				}
			}
			return nullptr;
		}();
		check(IsValid(AreaMeshComponent));

		TracingMeshComponent = GetOwner()->FindComponentByClass<UTracingMeshComponent>();
		check(IsValid(TracingMeshComponent));

		TracingMeshComponent->FirstEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
		TracingMeshComponent->LastEdgeModifier.BindWeakLambda(
			this, [this](auto&... Vertices) { (AttachVertexToAreaBoundary(Vertices), ...); });
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

		const bool bOwnerIsInsideArea = AreaMeshComponent->IsInside(GetOwner()->GetActorLocation());

		if (TracingMeshComponent->IsTracing() && bOwnerIsInsideArea)
		{
			TracingMeshComponent->SetTracingEnabled(false);
			AreaMeshComponent->ExpandByCounterClockwisePath(FindPathThatYieldsLargestArea());
		}
		else if (!TracingMeshComponent->IsTracing() && !bOwnerIsInsideArea)
		{
			TracingMeshComponent->Reset();
			TracingMeshComponent->SetTracingEnabled(true);
		}
	}

	void AttachVertexToAreaBoundary(FVector2D& Vertex) const
	{
		Vertex = AreaMeshComponent->FindClosestPointOnBoundary2D(Vertex).PointOfIntersection;
	}

	FSegmentArray2D FindPathThatYieldsLargestArea() const
	{
		const FSegmentArray2D& CenterSegmentArray = TracingMeshComponent->GetCenterSegmentArray2D();
		
		if (CenterSegmentArray.IsStraight())
		{
			const FVector2D PointOfDeparture = CenterSegmentArray.GetPoints()[0];
			const UAreaMeshComponent::FIntersection Intersection = AreaMeshComponent->FindClosestPointOnBoundary2D(PointOfDeparture);
			const FVector2D PathDirection = CenterSegmentArray[0].Direction;
			const FVector2D HitSegmentDirection = Intersection.HitSegment.Points.Direction;
			const bool bCavityIsToTheRight = FVector2D::CrossProduct(PathDirection, HitSegmentDirection) > 0.;

			if (bCavityIsToTheRight)
			{
				FSegmentArray2D Ret = TracingMeshComponent->GetLeftSegmentArray2D();
				Ret.ReverseVertexOrder();
				return Ret;
			}
			
			return TracingMeshComponent->GetRightSegmentArray2D();
		}
		
		if (CenterSegmentArray.IsClockwise())
		{
			FSegmentArray2D Ret = TracingMeshComponent->GetLeftSegmentArray2D();
			Ret.ReverseVertexOrder();
			return Ret;
		}
		
		return TracingMeshComponent->GetRightSegmentArray2D();
	}
};
