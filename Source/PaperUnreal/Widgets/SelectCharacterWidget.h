// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectCharacterWidget.generated.h"

/**
 * 
 */
UCLASS()
class USelectCharacterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnManny;
	FSimpleMulticastDelegate OnQuinn;

private:
	UFUNCTION(BlueprintCallable)
	void OnMannySelected() { OnManny.Broadcast(); }

	UFUNCTION(BlueprintCallable)
	void OnQuinnSelected() { OnQuinn.Broadcast(); }
};
