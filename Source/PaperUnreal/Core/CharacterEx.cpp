// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterEx.h"

#include "Utils.h"
#include "GameFramework/PlayerState.h"

TWeakAwaitable<AController*> ACharacterEx::WaitForController()
{
	if (AController* ValidController = ValidOrNull(GetController()))
	{
		return ValidController;
	}

	return WaitForBroadcast(this, OnControllerChanged);
}

TWeakAwaitable<APlayerState*> ACharacterEx::WaitForPlayerState()
{
	if (APlayerState* ValidPlayerState = ValidOrNull(GetPlayerState()))
	{
		return ValidPlayerState;
	}

	return WaitForBroadcast(this, OnNewPlayerState);
}

void ACharacterEx::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	OnControllerChanged.Broadcast(GetController());
}

void ACharacterEx::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	OnNewPlayerState.Broadcast(NewPlayerState);
}
