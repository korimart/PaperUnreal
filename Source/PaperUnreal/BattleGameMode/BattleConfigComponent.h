// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "BattleConfigComponent.generated.h"


USTRUCT(Blueprintable)
struct FBattleConfig
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	int32 MaxTeamCount = 2;

	UPROPERTY(BlueprintReadWrite)
	int32 MaxMemberCount = 2;
};


UCLASS(Within=PlayerController)
class UBattleConfigComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	const FBattleConfig& GetConfig() const
	{
		check(GetNetMode() != NM_Client);
		return CurrentConfig;
	}
	
	TCancellableFuture<FBattleConfig> FetchConfig()
	{
		auto [Promise, Future] = MakePromise<FBattleConfig>();
		ClientPendingConfigs.Add(MoveTemp(Promise));
		ServerSendConfig();
		return MoveTemp(Future);
	}

	TCancellableFuture<bool> SendConfig(const FBattleConfig& Config)
	{
		auto [Promise, Future] = MakePromise<bool>();
		ClientPendingConfirmations.Add(MoveTemp(Promise));
		ServerReceiveConfig(Config);
		return MoveTemp(Future);
	}

private:
	FBattleConfig CurrentConfig;
	TArray<TCancellablePromise<FBattleConfig>> ClientPendingConfigs;
	TArray<TCancellablePromise<bool>> ClientPendingConfirmations;
	
	UBattleConfigComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION(Server, Reliable)
	void ServerSendConfig();
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveConfig(const FBattleConfig& Config);
	
	UFUNCTION(Server, Reliable)
	void ServerReceiveConfig(const FBattleConfig& Config);
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveConfirmation(bool bSucceeded);
};
