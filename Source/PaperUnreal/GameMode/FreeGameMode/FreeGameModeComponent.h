// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FreePawnComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/GameModeComponent.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterMeshFromInventory.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "FreeGameModeComponent.generated.h"


UCLASS()
class UFreeGameModeComponent : public UGameModeComponent
{
	GENERATED_BODY()

public:
	void Start()
	{
		PawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOwner());
		PawnSpawner->DestroyPawnsOnEndPlay();
		PawnSpawner->RegisterComponent();

		auto PlayerStateArray = GetWorld()->GetGameState<AGameStateBase2>()->GetPlayerStateArray();

		PlayerStateArray.ObserveAddIfValid(this, [this](APlayerState* Player)
		{
			InitiatePawnSpawnSequence(Player);
		});

		PlayerStateArray.ObserveRemoveIfValid(this, [this](APlayerState* Player)
		{
			if (APawn* Pawn = Player->GetPawn())
			{
				Pawn->Destroy();
			}
		});
	}

private:
	UPROPERTY()
	UPawnSpawnerComponent* PawnSpawner;

	virtual void AttachPlayerStateComponents(APlayerState* PS) override
	{
		NewChildComponent<UInventoryComponent>(PS)->RegisterComponent();
	}

	FWeakCoroutine InitiatePawnSpawnSequence(APlayerState* Player)
	{
		co_await AddToWeakList(Player);

		Player->FindComponentByClass<UInventoryComponent>()->SetTracerBaseColor(NonEyeSoaringRandomColor());

		co_await Player->FindComponentByClass<UReadyStateComponent>()->GetbReady().If(true);

		APawn* Pawn = PawnSpawner->SpawnAtLocation(
			GetDefaultPawnClass(),
			{1500.f, 1500.f, 100.f},
			[&](APawn* Spawned) { NewChildComponent<UFreePawnComponent>(Spawned)->RegisterComponent(); });

		Player->GetOwningController()->Possess(Pawn);
	}
};
