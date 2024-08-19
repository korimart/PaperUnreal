// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExitWidget.generated.h"

/**
 * 
 */
UCLASS()
class UExitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnExitPressed;

private:
	UFUNCTION(BlueprintCallable)
	void Exit()
	{
		OnExitPressed.Broadcast();
	}
};
