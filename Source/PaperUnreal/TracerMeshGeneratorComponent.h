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
	void SetMeshSource(ITracerPathStream* Source)
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

		check(AllValid(MeshSource, MeshDestination, MeshAttachmentTarget));

		ListenToNextTracerStream();
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
		if (MeshAttachmentTarget->IsValid())
		{
			auto [Left, Right] = MeshDestination->GetVertices(Index);
			Left = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Left).GetPoint();
			Right = MeshAttachmentTarget->FindClosestPointOnBoundary2D(Right).GetPoint();
			MeshDestination->SetVertices(Index, Left, Right);
		}
	}

	void ListenToNextTracerStream()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto PathStream = MeshSource->GetTracerPathStreamer().CreateStream();
			check((co_await PathStream.Next()).IsOpenedEvent());

			MeshDestination->Edit([&, FirstEvent = co_await PathStream.Next()]()
			{
				check(!FirstEvent.IsClosedEvent());
				MeshDestination->Reset();
				AppendVerticesFromTracerPath(FirstEvent.Affected);
				AttachVerticesToAttachmentTarget(0);
			});

			while (co_await PathStream.NextIfNotEnd())
			{
				const auto Event = PathStream.Pop();

				if (Event.IsAppendedEvent())
				{
					MeshDestination->Edit([&]() { AppendVerticesFromTracerPath(Event.Affected); });
				}
				else if (Event.IsLastModifiedEvent())
				{
					// 현재 이 클래스의 가정: 수정은 Path의 가장 마지막 점에 대해서만 발생. 이 전제가 바뀌면 로직 수정해야 됨
					check(Event.Affected.Num() == 1);
					MeshDestination->Edit([&]() { ModifyLastVerticesWithTracerPoint(Event.Affected[0]); });
				}
				else
				{
					break;
				}
			}

			MeshDestination->Edit([&]() { AttachVerticesToAttachmentTarget(-1); });

			// 스트림의 종료가 MeshSource의 파괴로 인한 것인지 검사한다
			// 스트림이 끝난 시점에서는 살아 있는 상태에서 Clean up 중인거라 MeshSource의 파괴 여부를 알 수 없음
			// 다음 틱에 Garbage가 되었는지 검사한다
			co_await WaitOneTick(GetWorld());
			
			if (AllValid(MeshSource))
			{
				ListenToNextTracerStream();
			}
			else
			{
				MeshDestination->Edit([&]() { MeshDestination->Reset(); });
			}
		});
	}
};
