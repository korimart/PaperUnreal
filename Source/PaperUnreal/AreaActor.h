// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TeamComponent.h"
#include "GameFramework/Actor.h"
#include "AreaActor.generated.h"


UCLASS()
class AAreaActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UAreaMeshComponent* AreaMeshComponent;
	
	UPROPERTY()
	UTeamComponent* TeamComponent;

private:
	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		AreaMeshComponent = CreateDefaultSubobject<UAreaMeshComponent>(TEXT("AreaMeshComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}
};
