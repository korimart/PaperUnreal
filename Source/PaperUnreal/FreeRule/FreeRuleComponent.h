// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FreeRulePawnComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/GameStateBase2.h"
#include "PaperUnreal/ModeAgnostic/CharacterMeshFromInventory.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "FreeRuleComponent.generated.h"


UCLASS(Within=GameModeBase)
class UFreeRuleComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	void Start(UClass* InPawnClass)
	{
		PawnClass = InPawnClass;

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
	UClass* PawnClass;

	UPROPERTY()
	UPawnSpawnerComponent* PawnSpawner;

	void InitiatePawnSpawnSequence(APlayerState* Player)
	{
		RunWeakCoroutine(this, [this, Player](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(Player);

			Player->FindComponentByClass<UInventoryComponent>()->SetTracerBaseColor(NonEyeSoaringRandomColor());

			co_await Player->FindComponentByClass<UReadyStateComponent>()->GetbReady().If(true);

			APawn* Pawn = PawnSpawner->SpawnAtLocation(
				PawnClass,
				{1500.f, 1500.f, 100.f},
				[&](APawn* Spawned) { NewChildComponent<UFreeRulePawnComponent>(Spawned)->RegisterComponent(); });

			Player->GetOwningController()->Possess(Pawn);
		});
	}
};
