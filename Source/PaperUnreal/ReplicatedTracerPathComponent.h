// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponent2.h"
#include "Net/UnrealNetwork.h"
#include "ReplicatedTracerPathComponent.generated.h"


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponent2, public ITracerPathGenerator
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPathEvent, FTracerPathEvent);
	FOnPathEvent OnPathEvent;

	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() override
	{
		check(TypedReplicator); // did you wait for init?
		return TypedReplicator->CreateTypedEventGenerator();
	}

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		ServerTracerPath = Source;
	}

	TWeakAwaitable<bool> WaitForClientInitComplete()
	{
		check(GetNetMode() == NM_Client);
		return CreateAwaitableToArray(IsValid(Replicator), true, InitCompleteHandles);
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;
	
	UPROPERTY(ReplicatedUsing=OnRep_Replicator)
	UByteArrayReplicatorComponent* Replicator;
	
	TSharedPtr<TTypedByteArrayReplicatorView<FTracerPathPoint>> TypedReplicator;
	TArray<TWeakAwaitableHandle<bool>> InitCompleteHandles;

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

		Replicator = NewObject<UByteArrayReplicatorComponent>(this);
		Replicator->RegisterComponent();
		TypedReplicator = MakeShared<TTypedByteArrayReplicatorView<FTracerPathPoint>>(*Replicator);

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto PathEvents = ServerTracerPath->CreatePathEventGenerator();;)
			{
				TypedReplicator->SetFromEvent(co_await PathEvents.Next());
			}
		});
	}

	UFUNCTION()
	void OnRep_Replicator()
	{
		if (IsValid(Replicator))
		{
			TypedReplicator = MakeShared<TTypedByteArrayReplicatorView<FTracerPathPoint>>(*Replicator);
			FlushAwaitablesArray(InitCompleteHandles, true);
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, Replicator, COND_InitialOnly);
	}
};
