// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "PlayerController2.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/WeakCoroutine/TransformAwaitable.h"
#include "HUD2.generated.h"

/**
 * 
 */
UCLASS()
class AHUD2 : public AHUD
{
	GENERATED_BODY()

public:
	template <typename T>
	auto GetOwningPlayerState() const
	{
		auto PC = CastChecked<APlayerController2>(GetOwningPlayerController());
		return PC->GetPlayerState() | Awaitables::Cast<T>();
	}

	UEnhancedInputComponent* GetEnhancedInputComponent() const
	{
		return CastChecked<UEnhancedInputComponent>(GetOwningPlayerController()->InputComponent);
	}
};
