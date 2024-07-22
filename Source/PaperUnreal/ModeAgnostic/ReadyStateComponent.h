// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "ReadyStateComponent.generated.h"


UCLASS()
class UReadyStateComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(bool, bReady, RepbReady);

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);

private:
	UPROPERTY(ReplicatedUsing=OnRep_bReady)
	bool RepbReady = false;

	UReadyStateComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_bReady() { bReady.OnRep(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbReady);
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
