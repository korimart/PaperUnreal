// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "PlayerCollisionStateComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/Utils.h"
#include "OffAreaTracerGenEnablerComponent.generated.h"


UCLASS()
class UOffAreaTracerGenEnablerComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnGenPreEnable;
	FSimpleMulticastDelegate OnGenPostDisable;
	
	void SetVertexGenerator(UTracerVertexGeneratorComponent* Generator)
	{
		TracerVertexGenerator = Generator;
	}

	void SetGenPreventionArea(UAreaMeshComponent* Area)
	{
		GenPreventionArea = Area;
	}

	void SetCollisionState(UPlayerCollisionStateComponent* CollisionState)
	{
		PlayerCollisionState = CollisionState;
	}

	UTracerVertexGeneratorComponent* GetControlledGenerator() const
	{
		return TracerVertexGenerator;
	}

private:
	UPROPERTY()
	UTracerVertexGeneratorComponent* TracerVertexGenerator;

	UPROPERTY()
	UAreaMeshComponent* GenPreventionArea;

	UPROPERTY()
	UPlayerCollisionStateComponent* PlayerCollisionState;

	UOffAreaTracerGenEnablerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AreAllValid(TracerVertexGenerator, GenPreventionArea, PlayerCollisionState));

		PlayerCollisionState->FindOrAddCollisionWith(GenPreventionArea).ObserveValid(this, [this](bool bCollides)
		{
			const bool bToEnable = !bCollides;
			const bool bToDisable = bCollides;
			
			if (bToEnable)
			{
				OnGenPreEnable.Broadcast();
			}
			
			TracerVertexGenerator->SetGenerationEnabled(!bCollides);

			if (bToDisable)
			{
				OnGenPostDisable.Broadcast();
			}
		});
	}
};
