// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "LifeComponent.generated.h"


UCLASS()
class ULifeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(bool, bAlive, RepbAlive);

private:
	UPROPERTY(ReplicatedUsing=OnRep_bAlive)
	bool RepbAlive = true;
	
	UFUNCTION()
	void OnRep_bAlive() { bAlive.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbAlive);
	}
	
	ULifeComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};