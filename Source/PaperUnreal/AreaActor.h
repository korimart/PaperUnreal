// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AreaActor.generated.h"

UCLASS()
class AAreaActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UAreaMeshComponent* AreaMeshComponent;

	int32 GetTeamIndex() const { return TeamIndex; }
	void SetTeamIndex(int32 Index) { TeamIndex = Index; }

private:
	int32 TeamIndex = 0;
	
	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		AreaMeshComponent = CreateDefaultSubobject<UAreaMeshComponent>(TEXT("AreaMeshComponent"));
	}
};
