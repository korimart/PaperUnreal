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
	virtual void HandleDisconnect(UWorld* World, UNetDriver* NetDriver) override;
};


/**
 * 
 */
UCLASS()
class UPaperUnrealGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	TOptional<ENetworkFailure::Type> GetLastNetworkFailure() const
	{
		return LastNetworkFailure;
	}
	
	TOptional<ENetworkFailure::Type> GetUnhandledNetworkFailure() const
	{
		return UnhandledNetworkFailure;
	}
	
	void OnNetworkFailureHandled()
	{
		UnhandledNetworkFailure.Reset();
	}

	void ClearErrors()
	{
		LastNetworkFailure.Reset();
	}

private:
	TOptional<ENetworkFailure::Type> UnhandledNetworkFailure;
	TOptional<ENetworkFailure::Type> LastNetworkFailure;

	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass() override
	{
		return UPaperUnrealOnlineSession::StaticClass();
	}

	UFUNCTION(BlueprintCallable)
	void SetNetworkFailure(ENetworkFailure::Type Failure)
	{
		LastNetworkFailure = Failure;
		UnhandledNetworkFailure = Failure;
	}
};
