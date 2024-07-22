// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor2.h"

void AActor2::BeginPlay()
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
