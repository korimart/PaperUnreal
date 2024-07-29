// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/PVPBattleGameMode/PVPBattleGameState.h"
#include "InGameCheats.generated.h"


/**
 * 
 */
UCLASS()
class UInGameCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	inline static bool bDoNotEndGameUntilCheat{true};
	
	inline static FSimpleMulticastDelegate OnStartGameByCheat{};
	inline static FSimpleMulticastDelegate OnEndGameByCheat{};

private:
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

	UFUNCTION(Exec)
	void SetReady(bool bReady)
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UReadyStateComponent>()->ServerSetReady(bReady);
	}

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetAllReady(bool bReady)
	{
		for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
		{
			Each->FindComponentByClass<UReadyStateComponent>()->SetbReady(bReady);
		}
	}

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void StartGame()
	{
		OnStartGameByCheat.Broadcast();
	}
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EndGame()
	{
		OnEndGameByCheat.Broadcast();
	}
};
