// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponentEx.h"
#include "ComponentRegistry.h"

void UActorComponentEx::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UComponentRegistry>()->OnComponentBeginPlay(this);
}
