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
	if (FMath::IsWithinInclusive(Config.MaxTeamCount, 1, 4)
		&& FMath::IsWithinInclusive(Config.MaxMemberCount, 1, 4))
	{
		CurrentConfig = Config;
		ClientReceiveConfirmation(true);
	}
	else
	{
		ClientReceiveConfirmation(false);
	}
}

void UBattleRuleConfigComponent::ClientReceiveConfirmation_Implementation(bool bSucceeded)
{
	PopFront(ClientPendingConfirmations).SetValue(bSucceeded);
}
