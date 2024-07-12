// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
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
	
	UPROPERTY()
	UAreaBoundaryComponent* ServerAreaBoundary;
	
	UPROPERTY()
	UAreaMeshComponent* ClientAreaMesh;

private:
	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}

	virtual void AttachServerMachineComponents() override
	{
		ServerAreaBoundary = NewObject<UAreaBoundaryComponent>(this);
		ServerAreaBoundary->ResetToStartingBoundary(GetActorLocation());
		ServerAreaBoundary->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto ReplicatedAreaBoundary = NewObject<UReplicatedAreaBoundaryComponent>(this);
			ReplicatedAreaBoundary->SetAreaBoundarySource(ServerAreaBoundary);
			ReplicatedAreaBoundary->RegisterComponent();
		}
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
			const int32 TeamIndex = co_await TeamComponent->GetTeamIndex().WaitForValue();

			ClientAreaMesh = NewObject<UAreaMeshComponent>(this);
			ClientAreaMesh->RegisterComponent();
			// ClientAreaMesh->ConfigureMaterialSet({RR->GetAreaMaterialFor(TeamIndex)});

			IAreaBoundaryStream* AreaBoundaryStream = nullptr;
			if (GetNetMode() == NM_Client)
			{
				AreaBoundaryStream = co_await WaitForComponent<UReplicatedAreaBoundaryComponent>(this);
			}
			else
			{
				AreaBoundaryStream = co_await WaitForComponent<UAreaBoundaryComponent>(this);
			}

			auto AreaMeshGenerator = NewObject<UAreaMeshGeneratorComponent>(this);
			AreaMeshGenerator->SetMeshSource(AreaBoundaryStream);
			AreaMeshGenerator->SetMeshDestination(ClientAreaMesh);
			AreaMeshGenerator->RegisterComponent();
		});
	}
};
