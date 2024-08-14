// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ThirdPersonTemplatePlayerController.h"
#include "PVPBattlePlayerController.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattlePlayerController : public AThirdPersonTemplatePlayerController
{
	GENERATED_BODY()
	
private:
	virtual void BeginPlay() override;
};