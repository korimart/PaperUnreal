// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TeamScoresWidget.h"
#include "Blueprint/UserWidget.h"
#include "BattleRuleResultWidget.generated.h"

/**
 * 
 */
UCLASS()
class UBattleRuleResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnConfirmed;
	
	UPROPERTY(meta=(BindWidget))
	UTeamScoresWidget* TeamScoresWidget;

	UFUNCTION(BlueprintImplementableEvent)
	void OnScoresSet();
};
