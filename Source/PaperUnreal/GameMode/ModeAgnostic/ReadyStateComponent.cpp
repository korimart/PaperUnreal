// Fill out your copyright notice in the Description page of Project Settings.


#include "ReadyStateComponent.h"

#include "GameFramework/PlayerState.h"

void UReadySetterComponent::ServerSetReady_Implementation(bool bReady)
{
	GetOuterAPlayerController()->PlayerState->FindComponentByClass<UReadyStateComponent>()->SetbReady(bReady);
}
