// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WeakCoroutine/CancellableFuture.h"
#include "Character2.generated.h"


UCLASS()
class ACharacter2 : public ACharacter
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnControllerChanged, AController*);
	FOnControllerChanged OnControllerChanged;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStateChanged, APlayerState*);
	FOnPlayerStateChanged OnNewPlayerState;
	
	TCancellableFuture<AController*> WaitForController();
	TCancellableFuture<APlayerState*> WaitForPlayerState();

	virtual void NotifyControllerChanged() override;
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
	virtual void BeginPlay() override;
	virtual void AttachServerMachineComponents() {}
	virtual void AttachPlayerMachineComponents() {}
};
