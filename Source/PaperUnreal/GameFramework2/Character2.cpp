// Fill out your copyright notice in the Description page of Project Settings.


#include "Character2.h"

#include "Utils.h"
#include "GameFramework/PlayerState.h"

TCancellableFuture<AController*> ACharacter2::WaitForController()
{
	if (AController* ValidController = ValidOrNull(GetController()))
	{
		return ValidController;
	}

	return MakeFutureFromDelegate(OnControllerChanged);
}

TCancellableFuture<APlayerState*> ACharacter2::WaitForPlayerState()
{
	if (APlayerState* ValidPlayerState = ValidOrNull(GetPlayerState()))
	{
		return ValidPlayerState;
	}

	return MakeFutureFromDelegate(OnNewPlayerState);
}

void ACharacter2::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	OnControllerChanged.Broadcast(GetController());
}

void ACharacter2::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	OnNewPlayerState.Broadcast(NewPlayerState);
}
