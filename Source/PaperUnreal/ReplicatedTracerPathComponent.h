// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "Net/UnrealNetwork.h"
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
	
	UPROPERTY(ReplicatedUsing=OnRep_Replicator)
	UByteStreamComponent* Replicator;
	
	TSharedPtr<TTypedStreamView<FTracerPathPoint>> TypedReplicator;

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

		Replicator = NewObject<UByteStreamComponent>(this);
		Replicator->RegisterComponent();
		TypedReplicator = MakeShared<TTypedStreamView<FTracerPathPoint>>(*Replicator);

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto PathEvents = ServerTracerPath->CreatePathStream();;)
			{
				TypedReplicator->TriggerEvent(co_await PathEvents.Next());
			}
		});
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		// TypedReplicator를 먼저 파괴하면Replicator의 종료 이벤트를 TypedReplicator가
		// 전달하지 않음 전달 이후 즉시 파괴하여 내부에 Dangling Pointer가 없게 한다
		Replicator->DestroyComponent();
		TypedReplicator = nullptr;
	}

	UFUNCTION()
	void OnRep_Replicator()
	{
		if (IsValid(Replicator))
		{
			TypedReplicator = MakeShared<TTypedStreamView<FTracerPathPoint>>(*Replicator);
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, Replicator, COND_InitialOnly);
	}
};
