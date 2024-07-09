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

				MeshDestination->Edit([&]()
				{
					if (Event.IsReset())
					{
						MeshDestination->Reset();
						AppendVerticesFromTracerPath(Event.Affected);
					}
					else if (Event.IsAppended())
					{
						AppendVerticesFromTracerPath(Event.Affected);
					}
					else if (Event.IsLastModified())
					{
						// 현재 이 클래스의 가정: 수정은 Path의 가장 마지막 점에 대해서만 발생. 이 전제가 바뀌면 로직 수정해야 됨
						check(Event.Affected.Num() == 1);
						ModifyLastVerticesWithTracerPoint(Event.Affected[0]);
					}
				});
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

	void AppendVerticesFromTracerPath(const TArray<FTracerPathPoint>& TracerPath)
	{
		for (const FTracerPathPoint& Each : TracerPath)
		{
			auto [Left, Right] = CreateVertexPositions(Each.Point, Each.PathDirection);
			if (MeshDestination->GetVertexCount() == 0)
			{
				AttachVerticesToAttachmentTarget(Left, Right);
			}
			MeshDestination->AppendVertices(Left, Right);
		}
	}

	void ModifyLastVerticesWithTracerPoint(const FTracerPathPoint& Point)
	{
		auto [Left, Right] = CreateVertexPositions(Point.Point, Point.PathDirection);
		MeshDestination->SetLastVertices(Left, Right);
	}
};
