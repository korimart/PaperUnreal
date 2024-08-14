// Fill out your copyright notice in the Description page of Project Settings.


#include "BattleConfigComponent.h"

#include "PaperUnreal/GameFramework2/Utils.h"

void UBattleConfigComponent::ServerSendConfig_Implementation()
{
	ClientReceiveConfig(CurrentConfig);
}

void UBattleConfigComponent::ClientReceiveConfig_Implementation(const FBattleConfig& Config)
{
	PopFront(ClientPendingConfigs).SetValue(Config);
}

void UBattleConfigComponent::ServerReceiveConfig_Implementation(const FBattleConfig& Config)
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

void UBattleConfigComponent::ClientReceiveConfirmation_Implementation(bool bSucceeded)
{
	PopFront(ClientPendingConfirmations).SetValue(bSucceeded);
}
