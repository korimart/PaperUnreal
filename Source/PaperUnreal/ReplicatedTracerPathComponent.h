// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "ReplicatedTracerPathComponent.generated.h"


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponent2, public ITracerPathStream
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPathEvent, FTracerPathEvent);
	FOnPathEvent OnPathEvent;

	virtual TValueStream<FTracerPathEvent> CreatePathStream() override
	{
		return TypedReplicator->CreateTypedEventStream();
	}

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		ServerTracerPath = Source;
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	UPROPERTY()
	UByteStreamComponent* Replicator;

	TSharedPtr<TTypedStreamView<FTracerPathPoint>> TypedReplicator;

	UReplicatedTracerPathComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() != NM_Client)
		{
			check(AllValid(ServerTracerPath));
			
			Replicator = NewObject<UByteStreamComponent>(this);
			Replicator->RegisterComponent();
			TypedReplicator = MakeShared<TTypedStreamView<FTracerPathPoint>>(*Replicator);

			RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
			{
				Context.AddToWeakList(TypedReplicator);
				for (auto PathEvents = ServerTracerPath->CreatePathStream();;)
				{
					const FTracerPathEvent Event = co_await PathEvents.Next();
					TypedReplicator->TriggerEvent(Event);
				}
			});
		}
		else
		{
			// TODO find by tag
			Replicator = GetOwner()->FindComponentByClass<UByteStreamComponent>();
			TypedReplicator = MakeShared<TTypedStreamView<FTracerPathPoint>>(*Replicator);
		}
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);
		
		// TypedReplicator를 먼저 파괴하면 Replicator의 종료 이벤트를 TypedReplicator가
		// 전달하지 않음 전달 이후 즉시 파괴하여 내부에 Dangling Pointer가 없게 한다
		Replicator->DestroyComponent();
		TypedReplicator = nullptr;
	}
};
