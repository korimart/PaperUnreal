// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "Character2.generated.h"


UCLASS()
class UCharacterMovementComponent2 : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnCorrected;

private:
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override
	{
		Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
		OnCorrected.Broadcast();
	}
};


UCLASS()
class ACharacter2 : public ACharacter
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnControllerChanged, AController*);
	FOnControllerChanged OnControllerChanged;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStateChanged, APlayerState*);
	FOnPlayerStateChanged OnNewPlayerState;

	ACharacter2(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer.SetDefaultSubobjectClass<UCharacterMovementComponent2>(CharacterMovementComponentName))
	{
	}

	TCancellableFuture<AController*> WaitForController();
	TCancellableFuture<APlayerState*> WaitForPlayerState();

	virtual void NotifyControllerChanged() override;
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
};