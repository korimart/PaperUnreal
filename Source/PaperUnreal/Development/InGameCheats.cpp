// Fill out your copyright notice in the Description page of Project Settings.


#include "InGameCheats.h"
#include "PaperUnreal/GameMode/PVPBattleGameMode/PVPBattleGameModeComponent.h"

void UInGameCheats::StartGame()
{
	GetWorld()->GetAuthGameMode()->FindComponentByClass<UPVPBattleGameModeComponent>()->OnGameStartConditionsMet.Broadcast();
}
