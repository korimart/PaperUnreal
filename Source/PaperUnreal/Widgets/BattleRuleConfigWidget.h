// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PaperUnreal/BattleRule/BattleRuleConfigComponent.h"
#include "BattleRuleConfigWidget.generated.h"

/**
 * 
 */
UCLASS()
class UBattleRuleConfigWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmit, bool);
	FOnSubmit OnSubmit;
	
	const FBattleRuleConfig& GetLastSubmittedConfig() const
	{
		check(LastSubmittedConfig.IsSet());
		return *LastSubmittedConfig;
	}
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitialConfig(const FBattleRuleConfig& Config);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnSubmitFailed();
	
private:
	TOptional<FBattleRuleConfig> LastSubmittedConfig;
	
	UFUNCTION(BlueprintCallable)
	void Submit(const FBattleRuleConfig& Config)
	{
		LastSubmittedConfig = Config;
		OnSubmit.Broadcast(true);
	}
	
	UFUNCTION(BlueprintCallable)
	void Cancel()
	{
		OnSubmit.Broadcast(false);
	}
};