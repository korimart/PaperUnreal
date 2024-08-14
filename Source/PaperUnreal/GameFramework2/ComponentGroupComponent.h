// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "ComponentGroupComponent.generated.h"


UCLASS(Abstract)
class UComponentGroupComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	template <typename T>
	T* NewChildComponent()
	{
		return NewChildComponent<T>(GetOwner());
	}

	template <typename T, typename... ArgTypes>
	T* NewChildComponent(AActor* Actor, ArgTypes&&... Args)
	{
		T* Component = NewObject<T>(Actor, Forward<ArgTypes>(Args)...);
		Component->AddLifeDependency(this);
		OnNewChildComponent(Component);
		return Component;
	}

protected:
	UComponentGroupComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			AttachServerMachineComponents();
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			AttachPlayerMachineComponents();
		}
	}

	virtual void AttachServerMachineComponents()
	{
	}

	virtual void AttachPlayerMachineComponents()
	{
	}

	virtual void OnNewChildComponent(UActorComponent2* Component)
	{
	}
};
