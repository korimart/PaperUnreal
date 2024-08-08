// Fill out your copyright notice in the Description page of Project Settings.


#include "PVPBattlePlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

void APVPBattlePlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (GetNetMode() != NM_Client)
	{
		ServerPrivilegeComponent = NewObject<UPrivilegeComponent>(this);
		ServerPrivilegeComponent->RegisterComponent();
	}
}

void APVPBattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		RunWeakCoroutine(this, [this, Subsystem]() -> FWeakCoroutine
		{
			TOptional<FWeakCoroutine> WaitingForDeath;

			for (auto PawnStream = GetPossessedPawn().CreateStream();;)
			{
				auto PossessedPawn = co_await PawnStream;

				if (WaitingForDeath)
				{
					WaitingForDeath->Abort();
					WaitingForDeath.Reset();
				}

				if (!PossessedPawn || PossessedPawn->IsA<ASpectatorPawn>())
				{
					Subsystem->RemoveMappingContext(DefaultMappingContext);
					continue;
				}

				if (ULifeComponent* LifeComponent = PossessedPawn->FindComponentByClass<ULifeComponent>())
				{
					WaitingForDeath = RunWeakCoroutine(this, [this, LifeComponent, Subsystem]() -> FWeakCoroutine
					{
						co_await AddToWeakList(LifeComponent);

						Subsystem->AddMappingContext(DefaultMappingContext, 0);
						co_await LifeComponent->GetbAlive().If(false);
						Subsystem->RemoveMappingContext(DefaultMappingContext);
					});
				}
				else
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
			}
		});
	}

	EnableCheats();
	CheatManager->AddCheatManagerExtension(NewObject<UInGameCheats>(CheatManager));
}
