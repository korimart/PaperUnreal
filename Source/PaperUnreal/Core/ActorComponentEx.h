// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponentEx.generated.h"


UCLASS()
class UActorComponentEx : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};