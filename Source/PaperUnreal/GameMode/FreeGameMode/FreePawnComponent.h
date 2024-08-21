// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FreePlayerStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterMeshFeeder.h"
#include "FreePawnComponent.generated.h"


UCLASS(Within=Character2)
class UFreePawnComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	TLiveDataView<UTracerComponent*&> GetTracer() const
	{
		return Tracer;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_Tracer)
	UTracerComponent* RepTracer;
	TLiveData<UTracerComponent*&> Tracer{RepTracer};

	UFUNCTION()
	void OnRep_Tracer() { Tracer.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracer, COND_InitialOnly);
	}

	virtual void AttachServerMachineComponents() override
	{
		Tracer = NewChildComponent<UTracerComponent>();
		Tracer->RegisterComponent();
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto PlayerState = co_await GetOuterACharacter2()->WaitForPlayerState();
			auto PlayerStateComponent = co_await WaitForComponent<UFreePlayerStateComponent>(PlayerState);
			auto Inventory = co_await PlayerStateComponent->GetInventoryComponent();

			auto MeshFeeder = NewChildComponent<UCharacterMeshFeeder>();
			MeshFeeder->SetMeshStream(Inventory->GetCharacterMesh().MakeStream());
			MeshFeeder->RegisterComponent();

			co_await Tracer;
			Tracer->SetTracerColorStream(Inventory->GetTracerBaseColor().MakeStream());
		});
	}
};
