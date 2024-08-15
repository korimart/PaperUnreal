// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "InGameCheats.generated.h"


/**
 * 
 */
UCLASS()
class UInGameCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	inline static bool bDoNotEndGameUntilCheat{false};
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

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetReady(bool bReady)
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UReadyStateComponent>()->SetbReady(bReady);
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
	void EndGame()
	{
		OnEndGameByCheat.Broadcast();
	}

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetMannyInInventory()
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UInventoryComponent>()->SetCharacterMesh(
			TSoftObjectPtr<USkeletalMesh>
			{
				FSoftObjectPath{TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny'")}
			});
	}

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetQuinnInInventory()
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UInventoryComponent>()->SetCharacterMesh(
			TSoftObjectPtr<USkeletalMesh>
			{
				FSoftObjectPath{TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn'")}
			});
	}

	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetTracerColorInInventory(float R, float G, float B)
	{
		GetPlayerController()->PlayerState->FindComponentByClass<UInventoryComponent>()->SetTracerBaseColor(FLinearColor{R, G, B});
	}
};
