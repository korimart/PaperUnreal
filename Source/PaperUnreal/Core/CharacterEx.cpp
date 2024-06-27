// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterEx.h"

#include "Utils.h"

TWeakAwaitable<AController*> ACharacterEx::WaitForController()
{
	if (AController* ValidController = ValidOrNull(GetController()))
	{
		return ValidController;
	}

	return WaitForBroadcast(this, OnControllerChanged);
}

void ACharacterEx::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	OnControllerChanged.Broadcast(GetController());
}
