// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "PawnNoMovementComponent.generated.h"


UCLASS()
class UPawnNoMovementComponent : public USpectatorPawnMovement
{
	GENERATED_BODY()

public:
	UPawnNoMovementComponent()
	{
		MaxSpeed = 0.f;
	}
};
