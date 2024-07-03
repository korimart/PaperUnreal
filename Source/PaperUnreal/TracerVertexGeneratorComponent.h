// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/Utils.h"
#include "TracerVertexGeneratorComponent.generated.h"


UCLASS()
class UTracerVertexGeneratorComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void SetVertexSource(UTracerPathComponent* Source)
	{
		VertexSource = Source;
	}

	void SetVertexDestination(UTracerMeshComponent* Destination)
	{
		VertexDestination = Destination;
	}

	void SetVertexAttachmentTarget(UAreaMeshComponent* Target)
	{
		VertexAttachmentTarget = Target;
	}

private:
	UPROPERTY()
	UTracerPathComponent* VertexSource;

	UPROPERTY()
	UTracerMeshComponent* VertexDestination;

	UPROPERTY()
	UAreaMeshComponent* VertexAttachmentTarget;

	UTracerVertexGeneratorComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(VertexSource, VertexDestination, VertexAttachmentTarget));

		VertexSource->OnNewPointGenerated.AddWeakLambda(this, [this]()
		{
			auto [Left, Right] = CreateVertexPositions(
				VertexSource->GetPath().GetLastPoint(), FVector2D{GetOwner()->GetActorRightVector()}.GetSafeNormal());
			if (VertexSource->GetPath().PointCount() == 1)
			{
				Left = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
				Right = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
			}
			VertexDestination->AppendVertices(Left, Right);
		});

		VertexSource->OnLastPointModified.AddWeakLambda(this, [this]()
		{
			const auto [Left, Right] = CreateVertexPositions(
				VertexSource->GetPath().GetLastPoint(), FVector2D{GetOwner()->GetActorRightVector()}.GetSafeNormal());
			VertexDestination->SetLastVertices(Left, Right);
		});

		VertexSource->OnGenerationEnded.AddWeakLambda(this, [this]()
		{
			auto [Left, Right] = CreateVertexPositions(
				VertexSource->GetPath().GetLastPoint(), FVector2D{GetOwner()->GetActorRightVector()}.GetSafeNormal());
			Left = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
			Right = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
			VertexDestination->SetLastVertices(Left, Right);
		});

		VertexSource->OnCleared.AddWeakLambda(this, [this]()
		{
			VertexDestination->Reset();
		});
	}

	static std::tuple<FVector2D, FVector2D> CreateVertexPositions(const FVector2D& Center, const FVector2D& Right)
	{
		return {Center - 5.f * Right, Center + 5.f * Right};
	}
};
