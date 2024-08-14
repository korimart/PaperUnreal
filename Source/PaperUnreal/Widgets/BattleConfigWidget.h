// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleConfigComponent.h"
#include "BattleConfigWidget.generated.h"

/**
 * 
 */
UCLASS()
class UBattleConfigWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmit, bool);
	FOnSubmit OnSubmit;
	
	const FBattleConfig& GetLastSubmittedConfig() const
	{
		check(LastSubmittedConfig.IsSet());
		return *LastSubmittedConfig;
	}
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitialConfig(const FBattleConfig& Config);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnSubmitFailed();
	
private:
	TOptional<FBattleConfig> LastSubmittedConfig;
	
	UFUNCTION(BlueprintCallable)
	void Submit(const FBattleConfig& Config)
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