// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "Vector2DArrayReplicatorComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/ComponentRegistry.h"
#include "ReplicatedAreaBoundaryComponent.generated.h"


UCLASS()
class UReplicatedAreaBoundaryComponent : public UActorComponent2, public IVectorArray2DEventGenerator
{
	GENERATED_BODY()

public:
	virtual TValueGenerator<FVector2DArrayEvent> CreateEventGenerator() override
	{
		check(IsValid(Replicator)); // did you wait for init complete?
		return Replicator->CreateEventGenerator();
	}

	void SetAreaBoundarySource(UAreaBoundaryComponent* Source)
	{
		BoundarySource = Source;
	}

	TWeakAwaitable<bool> WaitForClientInitComplete()
	{
		check(GetNetMode() == NM_Client);
		return CreateAwaitableToArray(IsValid(Replicator), true, InitAwaitableHandles);
	}

private:
	UPROPERTY()
	UAreaBoundaryComponent* BoundarySource;

	UPROPERTY(ReplicatedUsing=OnRep_Replicator)
	UVector2DArrayReplicatorComponent* Replicator;

	TArray<TWeakAwaitableHandle<bool>> InitAwaitableHandles;

	UReplicatedAreaBoundaryComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() == NM_Client)
		{
			return;
		}

		check(AllValid(BoundarySource));

		Replicator = NewObject<UVector2DArrayReplicatorComponent>(this);
		Replicator->RegisterComponent();

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto Events = BoundarySource->CreateEventGenerator();;)
			{
				Replicator->SetFromEvent(co_await Events.Next());
			}
		});
	}

	UFUNCTION()
	void OnRep_Replicator()
	{
		if (IsValid(Replicator))
		{
			FlushAwaitablesArray(InitAwaitableHandles, true);
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, Replicator, COND_InitialOnly);
	}
};
