// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TeamComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnrealPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class APaperUnrealPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTeamComponent* TeamComponent;

private:
	APaperUnrealPlayerState()
	{
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}
};
