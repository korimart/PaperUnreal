// Fill out your copyright notice in the Description page of Project Settings.


#include "BattleRuleConfigComponent.h"

#include "PaperUnreal/GameFramework2/Utils.h"

void UBattleRuleConfigComponent::ServerSendConfig_Implementation()
{
	ClientReceiveConfig(CurrentConfig);
}

void UBattleRuleConfigComponent::ClientReceiveConfig_Implementation(const FBattleRuleConfig& Config)
{
	PopFront(ClientPendingConfigs).SetValue(Config);
}

void UBattleRuleConfigComponent::ServerReceiveConfig_Implementation(const FBattleRuleConfig& Config)
{
	// TODO validate
	CurrentConfig = Config;
	ClientReceiveConfirmation(true);
}

void UBattleRuleConfigComponent::ClientReceiveConfirmation_Implementation(bool bSucceeded)
{
	PopFront(ClientPendingConfirmations).SetValue(bSucceeded);
}