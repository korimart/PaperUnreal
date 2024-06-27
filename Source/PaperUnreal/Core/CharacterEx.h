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
	
	TWeakAwaitable<AController*> WaitForController();

	virtual void NotifyControllerChanged() override;
};
