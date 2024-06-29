// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"
#include "GameFramework/Character.h"
#include "CharacterEx.generated.h"


UCLASS()
class ACharacterEx : public ACharacter
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnControllerChanged, AController*);
	FOnControllerChanged OnControllerChanged;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStateChanged, APlayerState*);
	FOnPlayerStateChanged OnNewPlayerState;
	
	TWeakAwaitable<AController*> WaitForController();
	TWeakAwaitable<APlayerState*> WaitForPlayerState();

	virtual void NotifyControllerChanged() override;
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
	virtual void BeginPlay() override;
	virtual void AttachPlayerMachineComponents() {}
	virtual void AttachServerMachineComponents() {}
};
