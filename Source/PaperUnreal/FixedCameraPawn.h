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

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		// 에디터에서 적당히 카메라를 움직여 결정한 값
		SetActorLocation({170.449452f, 3101.368152f, 1058.444710f});
		SetActorRotation({-40.f, -50.f, 0.f});
	}
};
