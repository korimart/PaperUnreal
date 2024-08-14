// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "GameStarterComponent.generated.h"


UCLASS(Within=PlayerController)
class UGameStarterComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_RetVal(bool, FGameStarter);
	FGameStarter ServerGameStarter;
	
	TCancellableFuture<bool> StartGame()
	{
		auto [Promise, Future] = MakePromise<bool>();
		ClientPendingResponses.Add(MoveTemp(Promise));
		ServerStartGame();
		return MoveTemp(Future);
	}

private:
	TArray<TCancellablePromise<bool>> ClientPendingResponses;

	UGameStarterComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION(Server, Reliable)
	void ServerStartGame();

	UFUNCTION(Client, Reliable)
	void ClientStartGameResponse(bool bStarted);
};
