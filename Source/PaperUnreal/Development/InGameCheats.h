// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/BattleGameMode/PaperUnrealGameState.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "InGameCheats.generated.h"


/**
 * 
 */
UCLASS()
class UInGameCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	inline static FSimpleMulticastDelegate OnStartGameByCheat{};

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
		}
	}
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void KillArea(int32 TeamIndex)
	{
		for (TActorIterator<AAreaActor> It{GetWorld()}; It; ++It)
		{
			if (UTeamComponent* Team = It->FindComponentByClass<UTeamComponent>())
			{
				if (Team->GetTeamIndex().Get() == TeamIndex)
				{
					It->LifeComponent->SetbAlive(false);
				}
			}
		}
	}
	
	UFUNCTION(Exec)
	void SetReady(bool bReady)
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UReadyStateComponent>()->ServerSetReady(bReady);
	}
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void StartGame()
	{
		OnStartGameByCheat.Broadcast();
	}
};
