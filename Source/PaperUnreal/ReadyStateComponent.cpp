﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "ReadyStateComponent.h"

void UReadyStateComponent::ServerSetReady_Implementation(bool bNewReady)
{
	SetbReady(bNewReady);
}
