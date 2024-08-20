// Fill out your copyright notice in the Description page of Project Settings.


#include "PaperUnrealGameInstance.h"

void UPaperUnrealOnlineSession::HandleDisconnect(UWorld* World, UNetDriver* NetDriver)
{
	if (auto NetworkFailer = World->GetGameInstance<UPaperUnrealGameInstance>()->GetUnhandledNetworkFailure())
	{
		World->GetGameInstance<UPaperUnrealGameInstance>()->OnNetworkFailureHandled();
		Super::HandleDisconnect(World, NetDriver);
	}
	else
	{
		GEngine->CancelAllPending();
	}
}
