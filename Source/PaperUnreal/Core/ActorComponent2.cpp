// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent2.h"
#include "ComponentRegistry.h"

void UActorComponent2::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UComponentRegistry>()->OnComponentBeginPlay(this);
}
