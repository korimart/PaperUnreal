// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Actor2.generated.h"

UCLASS()
class AActor2 : public AActor
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void AttachPlayerMachineComponents() {}
	virtual void AttachServerMachineComponents() {}
};
