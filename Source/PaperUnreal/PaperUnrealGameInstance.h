// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/OnlineSession.h"
#include "PaperUnrealGameInstance.generated.h"


UCLASS()
class UPaperUnrealOnlineSession : public UOnlineSession
{
	GENERATED_BODY()

public:
	virtual void HandleDisconnect(UWorld *World, class UNetDriver *NetDriver) override
	{
		GEngine->CancelAllPending();
	}
};


/**
 * 
 */
UCLASS()
class UPaperUnrealGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass() override
	{
		return UPaperUnrealOnlineSession::StaticClass();
	}
};
