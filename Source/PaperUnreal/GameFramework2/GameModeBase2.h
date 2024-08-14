// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameStateBase2.h"
#include "PlayerController2.h"
#include "GameFramework/GameModeBase.h"
#include "GameModeBase2.generated.h"

/**
 * 
 */
UCLASS()
class AGameModeBase2 : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameModeBase2()
	{
		GameStateClass = AGameStateBase2::StaticClass();
		PlayerControllerClass = APlayerController2::StaticClass();
	}
};
