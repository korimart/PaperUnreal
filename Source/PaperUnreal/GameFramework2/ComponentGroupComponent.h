// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "ComponentGroupComponent.generated.h"


UCLASS(Abstract)
class UComponentGroupComponent : public UActorComponent2
{
	GENERATED_BODY()

protected:
	UComponentGroupComponent()
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

	virtual void AttachServerMachineComponents() {}
	virtual void AttachPlayerMachineComponents() {}
	
	template <typename T>
	T* NewChildComponent(AActor* Actor)
	{
		T* Component = NewObject<T>(Actor);
		Component->AddLifeDependency(this);
		return Component;
	}
};
