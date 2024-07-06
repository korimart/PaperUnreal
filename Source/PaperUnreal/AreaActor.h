// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "ResourceRegistryComponent.h"
#include "TeamComponent.h"
#include "Core/Actor2.h"
#include "Core/ComponentRegistry.h"
#include "AreaActor.generated.h"


UCLASS()
class AAreaActor : public AActor2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTeamComponent* TeamComponent;

private:
	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
			UResourceRegistryComponent* RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);

			// TODO graceful exit
			check(co_await RR->GetbResourcesLoaded().WaitForValue(this));

			const int32 TeamIndex = co_await TeamComponent->GetTeamIndex().WaitForValue(this);

			auto AreaMeshComponent = NewObject<UAreaMeshComponent>(this);
			AreaMeshComponent->RegisterComponent();
			AreaMeshComponent->ConfigureMaterialSet({RR->GetAreaMaterialFor(TeamIndex)});

			if (GetNetMode() == NM_Client)
			{
			}
			else
			{
				auto AreaBoundaryComponent = co_await WaitForComponent<UAreaBoundaryComponent>(this);
				AreaMeshComponent->SetMeshByWorldBoundary(AreaBoundaryComponent->GetBoundary());
				AreaBoundaryComponent->OnBoundaryChanged.AddWeakLambda(AreaMeshComponent,
					[AreaMeshComponent]<typename T>(T&& Boundary)
					{
						AreaMeshComponent->SetMeshByWorldBoundary(Forward<T>(Boundary));
					});
			}
		});
	}

	virtual void AttachServerMachineComponents() override
	{
		auto AreaBoundaryComponent = NewObject<UAreaBoundaryComponent>(this);
		AreaBoundaryComponent->ResetToStartingBoundary(GetActorLocation());
		AreaBoundaryComponent->RegisterComponent();
	}
};
