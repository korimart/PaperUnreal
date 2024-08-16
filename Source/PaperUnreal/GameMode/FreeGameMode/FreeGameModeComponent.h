// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FreePawnComponent.h"
#include "FreePlayerStateComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/GameModeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/KillZComponent.h"
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
		check(!bStarted.Get());
		PawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOwner());
		PawnSpawner->RegisterComponent();
		bStarted = true;
	}

private:
	UPROPERTY()
	UPawnSpawnerComponent* PawnSpawner;

	TLiveData<bool> bStarted;

	virtual void OnPostLogin(APlayerController* PC) override
	{
		// 디펜던시: parent game mode에서 미리 만들었을 것이라고 가정함
		check(!!PC->PlayerState->FindComponentByClass<UReadyStateComponent>());

		NewChildComponent<UFreePlayerStateComponent>(PC->PlayerState)->RegisterComponent();

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
		co_await (bStarted.MakeStream() | Awaitables::If(true));

		Player
			->PlayerState
			->FindComponentByClass<UFreePlayerStateComponent>()
			->GetInventoryComponent().Get()
			->SetTracerBaseColor(NonEyeSoaringRandomColor());

		co_await Player->PlayerState->FindComponentByClass<UReadyStateComponent>()->GetbReady().If(true);

		APawn* Pawn = PawnSpawner->SpawnAtLocation(
			GetDefaultPawnClass(),
			{1500.f, 1500.f, 100.f},
			[&](APawn* Spawned) { NewChildComponent<UFreePawnComponent>(Spawned)->RegisterComponent(); });

		Player->Possess(Pawn);

		while (true)
		{
			auto KillZComponent = NewChildComponent<UKillZComponent>(Pawn);
			KillZComponent->RegisterComponent();
			co_await KillZComponent->OnKillZ;
			Pawn->FindComponentByClass<UFreePawnComponent>()->GetTracer().Get()->ServerTracerPath->ClearPath();
			Pawn->SetActorLocation({1500.f, 1500.f, 100.f});
		}
	}
};
