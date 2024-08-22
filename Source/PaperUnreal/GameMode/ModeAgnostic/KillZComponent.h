// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "KillZComponent.generated.h"


UCLASS()
class UKillZComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnKillZ;

private:
	UKillZComponent()
	{
		PrimaryComponentTick.bCanEverTick = true;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		
		if (GetOwner()->GetActorLocation().Z < -300.f)
		{
			OnKillZ.Broadcast();
		}
	}
};
