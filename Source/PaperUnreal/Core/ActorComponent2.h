// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent2.generated.h"


UCLASS()
class UActorComponent2 : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};