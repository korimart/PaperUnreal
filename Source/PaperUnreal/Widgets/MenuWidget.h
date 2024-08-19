// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuWidget.generated.h"

/**
 * 
 */
UCLASS()
class UMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnHostPressed;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnJoinPressed, const FString&);
	FOnJoinPressed OnJoinPressed;

private:
	UFUNCTION(BlueprintCallable)
	void Host()
	{
		OnHostPressed.Broadcast();
	}
	
	UFUNCTION(BlueprintCallable)
	void Join(const FString& Address)
	{
		OnJoinPressed.Broadcast(Address);
	}
};
