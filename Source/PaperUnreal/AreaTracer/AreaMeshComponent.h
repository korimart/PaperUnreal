﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/AreaTracer/SegmentArray.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/DynamicMeshComponent2.h"
#include "Generators/PlanarPolygonMeshGenerator.h"
#include "AreaMeshComponent.generated.h"


UCLASS()
class UAreaMeshComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	template <CLoopedSegmentArray2D T>
	void SetMeshByWorldBoundary(T&& WorldBoundary)
	{
		LastSetWorldBoundary = Forward<T>(WorldBoundary);

		if (!LastSetWorldBoundary.IsValid())
		{
			DynamicMeshComponent->GetDynamicMesh()->Reset();
			return;
		}

		FLoopedSegmentArray2D LocalBoundary = LastSetWorldBoundary;
		LocalBoundary.ApplyToEachPoint([&](FVector2D& Each) { Each = WorldToLocal2D(Each); });

		UE::Geometry::FPlanarPolygonMeshGenerator Generator;
		Generator.Polygon = UE::Geometry::FPolygon2d{LocalBoundary.GetPoints()};
		Generator.Generate();
		DynamicMeshComponent->GetMesh()->Copy(&Generator);
		DynamicMeshComponent->NotifyMeshUpdated();
	}

	void ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
	{
		// TODO 작동하지 않는 듯 하니 치트로 머티리얼 나중에 설정해서 확인해보자
		DynamicMeshComponent->ConfigureMaterialSet(NewMaterialSet);
		DynamicMeshComponent->NotifyMaterialSetUpdated();
	}

	bool IsValid() const
	{
		return LastSetWorldBoundary.IsValid();
	}

	FSimpleMulticastDelegate& GetOnMeshChanged() const
	{
		return DynamicMeshComponent->OnMeshChanged;
	}

	struct FPointOnBoundary
	{
		FSegment2D Segment;
		float Alpha;

		FVector2D GetPoint() const
		{
			return Segment.PointBetween(Alpha);
		}
	};

	FPointOnBoundary FindClosestPointOnBoundary2D(const FVector2D& Point) const
	{
		using FIntersection = FLoopedSegmentArray2D::FIntersection;
		const FIntersection Intersection = LastSetWorldBoundary.FindClosestPointTo(Point);

		FPointOnBoundary Ret;
		Ret.Segment = LastSetWorldBoundary[Intersection.SegmentIndex];
		Ret.Alpha = Intersection.Alpha;
		return Ret;
	}

private:
	UPROPERTY()
	UDynamicMeshComponent2* DynamicMeshComponent;

	FLoopedSegmentArray2D LastSetWorldBoundary;

	FVector2D WorldToLocal2D(const FVector2D& World2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().InverseTransformPosition(FVector{World2D, 0.f})};
	}

	FVector2D LocalToWorld2D(const FVector2D& Local2D) const
	{
		return FVector2D{GetOwner()->GetActorTransform().TransformPosition(FVector{Local2D, 0.f})};
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		DynamicMeshComponent = NewObject<UDynamicMeshComponent2>(GetOwner(), TEXT("DynamicMeshComponent"));
		DynamicMeshComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DynamicMeshComponent->RegisterComponent();
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		DynamicMeshComponent->DestroyComponent();
		DynamicMeshComponent = nullptr;
	}
};