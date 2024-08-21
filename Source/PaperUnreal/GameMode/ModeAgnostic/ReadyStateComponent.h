// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "ReadyStateComponent.generated.h"


UCLASS()
class UReadyStateComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(bReady);

private:
	UPROPERTY(ReplicatedUsing=OnRep_bReady)
	bool RepbReady = false;
	TLiveData<bool&> bReady{RepbReady};

	UFUNCTION()
	void OnRep_bReady() { bReady.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbReady);
	}

	UReadyStateComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		if (GetNetMode() != NM_Client)
		{
			SetbReady(false);
		}
	}
};


UCLASS(Within=PlayerController)
class UReadySetterComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

private:
	UReadySetterComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};