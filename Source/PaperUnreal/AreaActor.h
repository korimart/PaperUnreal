// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
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

			auto AreaMesh = NewObject<UAreaMeshComponent>(this);
			AreaMesh->RegisterComponent();
			AreaMesh->ConfigureMaterialSet({RR->GetAreaMaterialFor(TeamIndex)});

			IVectorArray2DEventGenerator* AreaBoundaryGenerator = nullptr;
			if (GetNetMode() == NM_Client)
			{
				AreaBoundaryGenerator = co_await WaitForComponent<UReplicatedAreaBoundaryComponent>(this);
			}
			else
			{
				AreaBoundaryGenerator = co_await WaitForComponent<UAreaBoundaryComponent>(this);
			}
			
			auto AreaMeshGenerator = NewObject<UAreaMeshGeneratorComponent>(this);
			AreaMeshGenerator->SetMeshSource(AreaBoundaryGenerator);
			AreaMeshGenerator->SetMeshDestination(AreaMesh);
			AreaMeshGenerator->RegisterComponent();
		});
	}

	virtual void AttachServerMachineComponents() override
	{
		auto AreaBoundaryComponent = NewObject<UAreaBoundaryComponent>(this);
		AreaBoundaryComponent->ResetToStartingBoundary(GetActorLocation());
		AreaBoundaryComponent->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto ReplicatedAreaBoundary = NewObject<UReplicatedAreaBoundaryComponent>(this);
			ReplicatedAreaBoundary->SetAreaBoundarySource(AreaBoundaryComponent);
			ReplicatedAreaBoundary->RegisterComponent();
		}
	}
};
