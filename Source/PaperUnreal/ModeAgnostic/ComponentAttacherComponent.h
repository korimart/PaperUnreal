// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "ComponentAttacherComponent.generated.h"


UCLASS()
class UComponentAttacherComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	UComponentAttacherComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}
	
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		
		if (GetNetMode() != NM_Client)
		{
			AttachServerMachineComponents();
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			AttachPlayerMachineComponents();
		}
	}

protected:
	virtual void AttachServerMachineComponents() {}
	virtual void AttachPlayerMachineComponents() {}
};
