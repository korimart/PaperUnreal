// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "BattleRuleConfigComponent.generated.h"


USTRUCT(Blueprintable)
struct FBattleRuleConfig
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	int32 MaxTeamCount = 2;

	UPROPERTY(BlueprintReadWrite)
	int32 MaxMemberCount = 2;
};


UCLASS(Within=PlayerController)
class UBattleRuleConfigComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	const FBattleRuleConfig& GetConfig() const
	{
		check(GetNetMode() != NM_Client);
		return CurrentConfig;
	}
	
	TCancellableFuture<FBattleRuleConfig> FetchConfig()
	{
		auto [Promise, Future] = MakePromise<FBattleRuleConfig>();
		ClientPendingConfigs.Add(MoveTemp(Promise));
		ServerSendConfig();
		return MoveTemp(Future);
	}

	TCancellableFuture<bool> SendConfig(const FBattleRuleConfig& Config)
	{
		auto [Promise, Future] = MakePromise<bool>();
		ClientPendingConfirmations.Add(MoveTemp(Promise));
		ServerReceiveConfig(Config);
		return MoveTemp(Future);
	}

private:
	FBattleRuleConfig CurrentConfig;
	TArray<TCancellablePromise<FBattleRuleConfig>> ClientPendingConfigs;
	TArray<TCancellablePromise<bool>> ClientPendingConfirmations;
	
	UBattleRuleConfigComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION(Server, Reliable)
	void ServerSendConfig();
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveConfig(const FBattleRuleConfig& Config);
	
	UFUNCTION(Server, Reliable)
	void ServerReceiveConfig(const FBattleRuleConfig& Config);
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveConfirmation(bool bSucceeded);
};
