// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "Core/Utils.h"
#include "TracerVertexGeneratorComponent.generated.h"


UCLASS()
class UTracerVertexGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetVertexSource(ITracerPathGenerator* Source)
	{
		VertexSource = Cast<UObject>(Source);
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
	TScriptInterface<ITracerPathGenerator> VertexSource;

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

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto PathEventGenerator = VertexSource->CreatePathEventGenerator();

			while (true)
			{
				const FTracerPathEvent Event = co_await PathEventGenerator.Next();

				if (Event.GenerationStarted())
				{
					VertexDestination->Reset();
				}
				else if (Event.NewPointGenerated())
				{
					auto [Left, Right] = CreateVertexPositions(*Event.Point, *Event.PathDirection);
					if (VertexDestination->GetVertexCount() == 0)
					{
						AttachVerticesToAttachmentTarget(Left, Right);
					}
					VertexDestination->AppendVertices(Left, Right);
				}
				else if (Event.LastPointModified())
				{
					auto [Left, Right] = CreateVertexPositions(*Event.Point, *Event.PathDirection);
					VertexDestination->SetLastVertices(Left, Right);
				}
				else if (Event.GenerationEnded())
				{
					auto [Left, Right] = VertexDestination->GetLastVertices();
					AttachVerticesToAttachmentTarget(Left, Right);
					VertexDestination->SetLastVertices(Left, Right);
				}
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
		if (VertexAttachmentTarget->IsValid())
		{
			Left = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
			Right = VertexAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
		}
	}
};
