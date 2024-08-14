// Fill out your copyright notice in the Description page of Project Settings.


#include "GameStarterComponent.h"

#include "PaperUnreal/GameFramework2/Utils.h"

void UGameStarterComponent::ServerStartGame_Implementation()
{
	ClientStartGameResponse(ServerGameStarter.IsBound() && ServerGameStarter.Execute());
}

void UGameStarterComponent::ClientStartGameResponse_Implementation(bool bStarted)
{
	PopFront(ClientPendingResponses).SetValue(bStarted);
}
