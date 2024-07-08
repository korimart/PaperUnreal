// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "Core/Utils.h"
#include "TracerMeshGeneratorComponent.generated.h"


UCLASS()
class UTracerMeshGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshSource(ITracerPathGenerator* Source)
	{
		MeshSource = Cast<UObject>(Source);
	}

	void SetMeshDestination(UTracerMeshComponent* Destination)
	{
		MeshDestination = Destination;
	}

	void SetMeshAttachmentTarget(UAreaMeshComponent* Target)
	{
		MeshAttachmentTarget = Target;
	}

private:
	UPROPERTY()
	TScriptInterface<ITracerPathGenerator> MeshSource;

	UPROPERTY()
	UTracerMeshComponent* MeshDestination;

	UPROPERTY()
	UAreaMeshComponent* MeshAttachmentTarget;

	UTracerMeshGeneratorComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(MeshSource, MeshDestination, MeshAttachmentTarget));

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			for (auto PathEventGenerator = MeshSource->CreatePathEventGenerator();;)
			{
				const FTracerPathEvent Event = co_await PathEventGenerator.Next();
				UE_LOG(LogTemp, Warning, TEXT("Event %d"), Event.Affected.Num());
				//
				// if (Event.IsReset())
				// {
				// 	MeshDestination->Reset();
				// }
				// else if (Event.IsAppended())
				// {
				// 	auto [Left, Right] = CreateVertexPositions(Event.Point(), Event.PathDirection());
				// 	if (MeshDestination->GetVertexCount() == 0)
				// 	{
				// 		AttachVerticesToAttachmentTarget(Left, Right);
				// 	}
				// 	MeshDestination->AppendVertices(Left, Right);
				// }
				// else if (Event.LastPointModified())
				// {
				// 	auto [Left, Right] = CreateVertexPositions(Event.Point(), Event.PathDirection());
				// 	MeshDestination->SetLastVertices(Left, Right);
				// }
				// else if (Event.GenerationEnded())
				// {
				// 	auto [Left, Right] = MeshDestination->GetLastVertices();
				// 	AttachVerticesToAttachmentTarget(Left, Right);
				// 	MeshDestination->SetLastVertices(Left, Right);
				// }
			}
		});
	}

	static std::tuple<FVector2D, FVector2D> CreateVertexPositions(const FVector2D& Center, const FVector2D& Forward)
	{
		const FVector2D Right{-Forward.Y, Forward.X};
		return {Center - 5.f * Right, Center + 5.f * Right};
	}

	void AttachVerticesToAttachmentTarget(FVector2D& Left, FVector2D& Right) const
	{
		if (MeshAttachmentTarget->IsValid())
		{
			Left = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
			Right = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
		}
	}
};
