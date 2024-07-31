// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleRuleConfigWidget.generated.h"

/**
 * 
 */
UCLASS()
class UBattleRuleConfigWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FConfigConfirmHandler, int32, int32);
	FConfigConfirmHandler ConfirmHandler;
	
	UFUNCTION(BlueprintCallable)
	bool OnConfigConfirmed(int32 MaxTeamCount, int32 MaxMemberCount)
	{
		return ConfirmHandler.Execute(MaxTeamCount, MaxMemberCount);
	}
};