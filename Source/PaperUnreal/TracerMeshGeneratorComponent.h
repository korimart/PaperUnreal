// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "TracerMeshGeneratorComponent.generated.h"


UCLASS()
class UTracerMeshGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshSource(ITracerPathStream* Source)
	{
		check(!HasBeenInitialized());
		MeshSource = Cast<UObject>(Source);
	}

	void SetMeshDestination(UTracerMeshComponent* Destination)
	{
		check(!HasBeenInitialized());
		MeshDestination = Destination;
	}

	void SetMeshAttachmentTarget(UAreaMeshComponent* Target)
	{
		check(!HasBeenInitialized());
		MeshAttachmentTarget = Target;
	}

private:
	UPROPERTY()
	TScriptInterface<ITracerPathStream> MeshSource;

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

		AddLifeDependency(CastChecked<UActorComponent2>(MeshSource.GetObject()));
		AddLifeDependency(MeshDestination);
		AddLifeDependency(MeshAttachmentTarget);
		
		ListenToTracerPathStream();
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		if (AllValid(MeshDestination))
		{
			MeshDestination->Edit([&]() { MeshDestination->Reset(); });
		}
	}

	static std::tuple<FVector2D, FVector2D> CreateVertexPositions(const FVector2D& Center, const FVector2D& Forward)
	{
		const FVector2D Right{-Forward.Y, Forward.X};
		return {Center - 5.f * Right, Center + 5.f * Right};
	}

	void AppendVerticesFromTracerPath(const TArray<FTracerPathPoint>& TracerPath) const
	{
		for (const FTracerPathPoint& Each : TracerPath)
		{
			auto [Left, Right] = CreateVertexPositions(Each.Point, Each.PathDirection);
			MeshDestination->AppendVertices(Left, Right);
		}
	}

	void ModifyLastVerticesWithTracerPoint(const FTracerPathPoint& Point) const
	{
		auto [Left, Right] = CreateVertexPositions(Point.Point, Point.PathDirection);
		MeshDestination->SetVertices(-1, Left, Right);
	}

	void AttachVerticesToAttachmentTarget(int32 Index) const
	{
		auto [Left, Right] = MeshDestination->GetVertices(Index);
		// TODO Area에 메시가 생길 때까지 기다려야 함
		if (MeshAttachmentTarget->IsValid())
		{
			Left = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
			Right = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
		}
		MeshDestination->SetVertices(Index, Left, Right);
	}

	void ListenToTracerPathStream()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			while (true)
			{
				// TODO await
				// auto PathStream = MeshSource->GetTracerPathStreamer().CreateStream();
				//
				// if (!co_await PathStream.NextIfNotEnd())
				// {
				// 	continue;
				// }
				//
				// MeshDestination->Edit([&, FirstEvent = PathStream.Pop()]()
				// {
				// 	MeshDestination->Reset();
				// 	AppendVerticesFromTracerPath(FirstEvent.Affected);
				// 	AttachVerticesToAttachmentTarget(0);
				// });
				//
				// while (co_await PathStream.NextIfNotEnd())
				// {
				// 	const auto Event = PathStream.Pop();
				//
				// 	if (Event.IsAppendedEvent())
				// 	{
				// 		MeshDestination->Edit([&]() { AppendVerticesFromTracerPath(Event.Affected); });
				// 	}
				// 	else if (Event.IsLastModifiedEvent())
				// 	{
				// 		// 현재 이 클래스의 가정: 수정은 Path의 가장 마지막 점에 대해서만 발생. 이 전제가 바뀌면 로직 수정해야 됨
				// 		check(Event.Affected.Num() == 1);
				// 		MeshDestination->Edit([&]() { ModifyLastVerticesWithTracerPoint(Event.Affected[0]); });
				// 	}
				// }
				//
				// MeshDestination->Edit([&]() { AttachVerticesToAttachmentTarget(-1); });
			}
		});
	}
};
