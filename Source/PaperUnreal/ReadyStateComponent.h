// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "ReadyStateComponent.generated.h"


UCLASS()
class UReadyStateComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER_WITH_DEFAULT(bool, bReady, false);

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
	void OnRep_bReady() { bReady = RepbReady; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	}
};
