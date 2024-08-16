// Fill out your copyright notice in the Description page of Project Settings.


#include "PVPBattlePlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/GameMode/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"


void APVPBattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		RunWeakCoroutine(this, [this, Subsystem]() -> FWeakCoroutine
		{
			co_await AddToWeakList(Subsystem);
			
			for (auto PawnStream = GetPawn2().MakeStream();;)
			{
				auto PossessedPawn = co_await PawnStream;

				if (!PossessedPawn || PossessedPawn->IsA<ASpectatorPawn>())
				{
					Subsystem->RemoveMappingContext(DefaultMappingContext);
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
