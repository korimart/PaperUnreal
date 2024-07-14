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
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			check(AllValid(ServerTracerPath));
			Replicator = NewObject<UByteStreamComponent>(this);
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			Replicator->RegisterComponent();
			ServerInitiatePathRelay();
		}
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() == NM_Client)
		{
			// TODO find by tag
			Replicator = GetOwner()->FindComponentByClass<UByteStreamComponent>();
			check(IsValid(Replicator))
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			ClientInitiatePathRelay();
		}
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			Replicator->DestroyComponent();
		}

		TracerPathStreamer.EndStreams();
	}

	void ServerInitiatePathRelay()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AddToWeakList(ServerTracerPath);

			Replicator->OpenStream();
			auto PathEvents = ServerTracerPath->GetTracerPathStreamer().CreateStream();
			while (co_await PathEvents.NextIfNotEnd())
			{
				Replicator->TriggerEvent(PathEvents.Pop().Serialize());
			}
			Replicator->CloseStream();

			ServerInitiatePathRelay();
		});
	}

	void ClientInitiatePathRelay()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AddToWeakList(Replicator);

			auto ByteEvents = Replicator->GetByteStreamer().CreateStream();
			while (co_await ByteEvents.NextIfNotEnd())
			{
				TracerPathStreamer.ReceiveValue(ByteEvents.Pop().Deserialize<FTracerPathEvent>());
			}
			TracerPathStreamer.EndStreams();
			
			ClientInitiatePathRelay();
		});
	}
};
