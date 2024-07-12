// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UECoroutine.h"
#include "GameFramework/PlayerController.h"
#include "PlayerController2.generated.h"

/**
 * 
 */
UCLASS()
class APlayerController2 : public APlayerController
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPossessedPawnChanged2, APawn*, APawn*);
	FOnPossessedPawnChanged2 OnPossessedPawnChanged2;

	TValueStream<APawn*> CreatePossessedPawnStream()
	{
		auto Init = GetPawn() ? TArray{GetPawn()} : TArray<APawn*>{};
		return CreateMulticastValueStream(Init, OnPossessedPawnChanged2, [](auto, APawn* New) { return New; });
	}

protected:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChangedDynamicCallback);
	}

private:
	UFUNCTION()
	void OnPossessedPawnChangedDynamicCallback(APawn* InOldPawn, APawn* InNewPawn)
	{
		OnPossessedPawnChanged2.Broadcast(InOldPawn, InNewPawn);
	}
};
