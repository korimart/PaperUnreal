// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedTracerPathComponent.generated.h"


USTRUCT()
struct FReplicatedTracerEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int32 GenerationId;

	UPROPERTY()
	ETracerPathEvent Event;

	UPROPERTY()
	FVector2D Point;

	UPROPERTY()
	FVector2D PathDirection;

	FReplicatedTracerEvent() = default;

	FReplicatedTracerEvent(int32 GenId, const FTracerPathEvent& PathEvent)
		: GenerationId(GenId)
	{
		Event = PathEvent.Event;

		if (PathEvent.Point)
		{
			Point = *PathEvent.Point;
		}

		if (PathEvent.PathDirection)
		{
			PathDirection = *PathEvent.PathDirection;
		}
	}

	operator FTracerPathEvent() const
	{
		FTracerPathEvent Ret{.Event = Event};

		if (Ret.NewPointGenerated() || Ret.LastPointModified())
		{
			Ret.Point = Point;
			Ret.PathDirection = PathDirection;
		}

		return Ret;
	}
};


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponent2, public ITracerPathGenerator
{
	GENERATED_BODY()

public:
	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() override
	{
		TArray<FTracerPathEvent> Initial;
		Algo::Copy(RepEvents, Initial);
		return CreateMulticastValueGenerator(this, Initial, OnPathEvent);
	}

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		ServerTracerPath = Source;
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	UPROPERTY(ReplicatedUsing=OnRep_Events)
	TArray<FReplicatedTracerEvent> RepEvents;

	int32 ServerNextGenerationId = 1;
	int32 ClientNextBroadcastIndex = 0;
	TOptional<ETracerPathEvent> ClientLastBroadcastEvent;

	UReplicatedTracerPathComponent()
	{
		SetIsReplicatedByDefault(true);
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() == NM_Client)
		{
			return;
		}

		check(AllValid(ServerTracerPath));

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto PathEventGenerator = ServerTracerPath->CreatePathEventGenerator();

			while (true)
			{
				const FTracerPathEvent Event = co_await PathEventGenerator.Next();

				if (Event.GenerationStarted())
				{
					RepEvents.Empty();
					ServerNextGenerationId++;
				}

				RepEvents.Emplace(ServerNextGenerationId, Event);
			}
		});
	}

	UFUNCTION()
	void OnRep_Events(const TArray<FReplicatedTracerEvent>& Prev)
	{
		const bool bNewGenerationStarted =
			Prev.Num() == 0
			|| RepEvents.Num() == 0
			|| Prev[0].GenerationId != RepEvents[0].GenerationId;
		
		if (bNewGenerationStarted)
		{
			ClientNextBroadcastIndex = 0;
		}

		// 항상 모든 Stream이 GenerationStarted로 시작해서 GenerationEnded로 끝나는 것을 보장한다
		// 서버에서 GenerationEnded 이후 바로 배열을 비워버리면 클라는 해당 이벤트를 받지 못함
		// 그러므로 그런 경우가 발생하면 여기서 GenerationEnded를 브로드캐스트 해준다
		if (bNewGenerationStarted && ClientLastBroadcastEvent && *ClientLastBroadcastEvent != ETracerPathEvent::GenerationEnded)
		{
			ClientLastBroadcastEvent = RepEvents[ClientNextBroadcastIndex].Event;
			OnPathEvent.Broadcast({.Event = ETracerPathEvent::GenerationEnded});
		}

		for (; ClientNextBroadcastIndex < RepEvents.Num(); ClientNextBroadcastIndex++)
		{
			ClientLastBroadcastEvent = RepEvents[ClientNextBroadcastIndex].Event;
			OnPathEvent.Broadcast(RepEvents[ClientNextBroadcastIndex]);
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepEvents);
	}
};
