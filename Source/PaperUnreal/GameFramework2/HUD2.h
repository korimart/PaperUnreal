// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "PlayerController2.h"
#include "GameFramework/HUD.h"
#include "HUD2.generated.h"

/**
 * 
 */
UCLASS()
class AHUD2 : public AHUD
{
	GENERATED_BODY()

public:
	auto GetOwningPawn2() const
	{
		return CastChecked<APlayerController2>(GetOwningPlayerController())->GetPawn2();
	}

	APawn* GetOwningSpectatorPawn() const
	{
		return GetOwningPlayerController()->GetSpectatorPawn();
	}
	
	auto GetOwningSpectatorPawn2() const
	{
		return CastChecked<APlayerController2>(GetOwningPlayerController())->GetSpectatorPawn2();
	}
	
	auto GetOwningPawnOrSpectator2() const
	{
		return CastChecked<APlayerController2>(GetOwningPlayerController())->GetPawnOrSpectator2();
	}

	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const
	{
		return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetOwningPlayerController()->GetLocalPlayer());
	}

	UEnhancedInputComponent* GetEnhancedInputComponent() const
	{
		return CastChecked<UEnhancedInputComponent>(GetOwningPlayerController()->InputComponent);
	}
};
