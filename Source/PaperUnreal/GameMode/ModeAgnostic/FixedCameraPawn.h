// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnNoMovementComponent.h"
#include "GameFramework/SpectatorPawn.h"
#include "FixedCameraPawn.generated.h"

UCLASS()
class AFixedCameraPawn : public ASpectatorPawn
{
	GENERATED_BODY()

private:
	AFixedCameraPawn(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer.SetDefaultSubobjectClass<UPawnNoMovementComponent>(MovementComponentName))
	{
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;
	}
	
	virtual FRotator GetViewRotation() const override
	{
		return GetActorRotation();
	}
};
