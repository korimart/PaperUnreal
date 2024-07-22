// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPathGenerator.h"
#include "ReplicatedTracerPathComponent.generated.h"


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponent2, public ITracerPathStream
{
	GENERATED_BODY()

public:
	virtual const TValueStreamer<FTracerPathEvent>& GetTracerPathStreamer() const override
	{
		return TracerPathStreamer;
	}

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerTracerPath = Source;
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	UPROPERTY()
	UByteStreamComponent* Replicator;

	TValueStreamer<FTracerPathEvent> TracerPathStreamer;

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
			AddLifeDependency(ServerTracerPath);
			Replicator = NewObject<UByteStreamComponent>(this);
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			Replicator->SetInputStream(ServerTracerPath->GetTracerPathStreamer());
			Replicator->RegisterComponent();
		}
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() == NM_Client)
		{
			// TODO find by tag
			Replicator = GetOwner()->FindComponentByClass<UByteStreamComponent>();
			Replicator->SetChunkSize(FTracerPathPoint::ChunkSize);
			Replicator->SetOutputStream(TracerPathStreamer);
		}
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			Replicator->DestroyComponent();
		}
	}
};
