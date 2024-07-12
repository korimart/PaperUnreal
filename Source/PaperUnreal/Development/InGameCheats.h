// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "PaperUnreal/PaperUnrealGameState.h"
#include "PaperUnreal/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/Core/Character2.h"
#include "InGameCheats.generated.h"

/**
 * 
 */
UCLASS()
class UInGameCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

private:
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SpawnAreaForTeam(int32 TeamIndex)
	{
		auto Spawned = GetWorld()
		               ->GetGameState<APaperUnrealGameState>()
		               ->AreaSpawnerComponent
		               ->SpawnAreaAtRandomEmptyLocation();
		
		Spawned->TeamComponent->SetTeamIndex(TeamIndex);
	}
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void DestroyAllCharacters()
	{
		for (TActorIterator<ACharacter2> It{GetWorld()}; It; ++It)
		{
			It->Destroy();
		}
	}
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void DestroyAllTracerPaths()
	{
		for (TActorIterator<ACharacter2> It{GetWorld()}; It; ++It)
		{
			if (UTracerPathComponent* Tracer = It->FindComponentByClass<UTracerPathComponent>())
			{
				Tracer->DestroyComponent();
			}
			
			if (UReplicatedTracerPathComponent* Tracer = It->FindComponentByClass<UReplicatedTracerPathComponent>())
			{
				Tracer->DestroyComponent();
			}
		}
	}
};
