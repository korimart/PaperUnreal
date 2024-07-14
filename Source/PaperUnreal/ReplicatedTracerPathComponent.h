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
	DECLARE_STREAMER_AND_GETTER(FTracerPathEvent, TracerPathStreamer);

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

	UReplicatedTracerPathComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() == NM_Client)
		{
			// TODO find by tag
			Replicator = GetOwner()->FindComponentByClass<UByteStreamComponent>();
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			ClientInitiatePathRelay();
		}
		else
		{
			check(AllValid(ServerTracerPath));
			Replicator = NewObject<UByteStreamComponent>(this);
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			Replicator->RegisterComponent();
			ServerInitiatePathRelay();
		}
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		if (GetNetMode() != NM_Client)
		{
			Replicator->DestroyComponent();
		}
	}

	void ServerInitiatePathRelay()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto PathEvents = ServerTracerPath->GetTracerPathStreamer().CreateStream();
			while (co_await PathEvents.NextIfNotEnd())
			{
				Replicator->TriggerEvent(PathEvents.Pop().Serialize());
			}
			ServerInitiatePathRelay();
		});
	}

	void ClientInitiatePathRelay()
	{
		TracerPathStreamer.EndStreams();
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto ByteEvents = Replicator->GetByteStreamer().CreateStream();
			while (co_await ByteEvents.NextIfNotEnd())
			{
				TracerPathStreamer.ReceiveValue(ByteEvents.Pop().Deserialize<FTracerPathEvent>());
			}
			ClientInitiatePathRelay();
		});
	}
};
