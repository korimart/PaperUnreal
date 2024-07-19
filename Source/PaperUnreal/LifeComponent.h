// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "Net/UnrealNetwork.h"
#include "LifeComponent.generated.h"


UCLASS()
class ULifeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER_WITH_DEFAULT(bool, bAlive, true);

	// TODO TWeakAwaitable<void>를 반환할 방법이 없는지 고민해본다
	TCancellableFuture<bool> WaitForDeath() { return GetbAlive().WaitForValue(false); }
	
private:
	UPROPERTY(ReplicatedUsing=OnRep_bAlive)
	bool RepbAlive = true;
	
	ULifeComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_bAlive() { bAlive = RepbAlive; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbAlive);
	}
};