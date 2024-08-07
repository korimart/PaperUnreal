// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/ModeAgnostic/PrivilegeComponent.h"
#include "PaperUnreal/ModeAgnostic/ThirdPersonTemplatePlayerController.h"
#include "PVPBattlePlayerController.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattlePlayerController : public AThirdPersonTemplatePlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UPrivilegeComponent* ServerPrivilegeComponent;

private:
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
};
