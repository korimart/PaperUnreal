// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "ResourceRegistryComponent.h"
#include "TeamComponent.h"
#include "Core/ComponentRegistry.h"
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

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() == NM_DedicatedServer)
		{
			return;
		}

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
			UResourceRegistryComponent* RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);

			if (!co_await RR->GetbResourcesLoaded().WaitForValue(this))
			{
				co_return;
			}

			const int32 TeamIndex = co_await TeamComponent->GetTeamIndex().WaitForValue(this);
			AreaMeshComponent->ConfigureMaterialSet({RR->GetAreaMaterialFor(TeamIndex)});
		});
	}
};
