// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PrivilegeComponent.generated.h"


UCLASS()
class UPrivilegeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(bHost);

private:
	UPROPERTY(ReplicatedUsing=OnRep_bHost)
	bool RepbHost = false;
	mutable TLiveData<bool&> bHost{RepbHost};

	UFUNCTION()
	void OnRep_bHost() { bHost.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepbHost);
	}

	UPrivilegeComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};