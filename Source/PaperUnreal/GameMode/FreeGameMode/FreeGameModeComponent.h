// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FreePawnComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/GameModeComponent.h"
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
		check(HasBegunPlay());
		PawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOwner());
		PawnSpawner->DestroyPawnsOnEndPlay();
		PawnSpawner->RegisterComponent();
	}

private:
	UPROPERTY()
	UPawnSpawnerComponent* PawnSpawner;

	virtual void OnPostLogin(APlayerController* PC) override
	{
		// 디펜던시: parent game mode에서 미리 만들었을 것이라고 가정함
		check(!!PC->PlayerState->FindComponentByClass<UReadyStateComponent>());
		
		NewChildComponent<UInventoryComponent>(PC->PlayerState)->RegisterComponent();
		
		InitiatePawnSpawnSequence(PC);
	}

	virtual void OnPostLogout(APlayerController* PC) override
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->Destroy();
		}
	}

	FWeakCoroutine InitiatePawnSpawnSequence(APlayerController* Player)
	{
		co_await AddToWeakList(Player);

		Player->PlayerState->FindComponentByClass<UInventoryComponent>()->SetTracerBaseColor(NonEyeSoaringRandomColor());

		co_await Player->PlayerState->FindComponentByClass<UReadyStateComponent>()->GetbReady().If(true);

		APawn* Pawn = PawnSpawner->SpawnAtLocation(
			GetDefaultPawnClass(),
			{1500.f, 1500.f, 100.f},
			[&](APawn* Spawned) { NewChildComponent<UFreePawnComponent>(Spawned)->RegisterComponent(); });

		Player->Possess(Pawn);
	}
};
