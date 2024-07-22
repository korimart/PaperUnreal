﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "LifeComponent.generated.h"


UCLASS()
class ULifeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(bool, bAlive, RepbAlive);

	// TODO TCancellableFuture<void>를 반환할 방법이 없는지 고민해본다
	TCancellableFuture<bool, EValueStreamError> WaitForDeath() { return GetbAlive().If(false); }
	
private:
	UPROPERTY(ReplicatedUsing=OnRep_bAlive)
	bool RepbAlive = true;
	
	ULifeComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_bAlive() { bAlive.OnRep(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbAlive);
	}
};