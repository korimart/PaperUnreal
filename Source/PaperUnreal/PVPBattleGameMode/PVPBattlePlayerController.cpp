// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPBattlePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CheatManager.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

APVPBattlePlayerController::APVPBattlePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void APVPBattlePlayerController::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		RunWeakCoroutine(this, [this, Subsystem](FWeakCoroutineContext&) -> FWeakCoroutine
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
					WaitingForDeath = RunWeakCoroutine(this, [this, LifeComponent, Subsystem](FWeakCoroutineContext& Context) -> FWeakCoroutine
					{
						Context.AbortIfNotValid(LifeComponent);

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

void APVPBattlePlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Setup mouse input events
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &APVPBattlePlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &APVPBattlePlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &APVPBattlePlayerController::OnSetDestinationReleased);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &APVPBattlePlayerController::OnSetDestinationReleased);

		// Setup touch input events
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &APVPBattlePlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &APVPBattlePlayerController::OnTouchTriggered);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &APVPBattlePlayerController::OnTouchReleased);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &APVPBattlePlayerController::OnTouchReleased);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void APVPBattlePlayerController::OnInputStarted()
{
	StopMovement();
}

// Triggered every frame when the input is held down
void APVPBattlePlayerController::OnSetDestinationTriggered()
{
	// We flag that the input is being pressed
	FollowTime += GetWorld()->GetDeltaSeconds();

	// We look for the location in the world where the player has pressed the input
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}

	// Move towards mouse pointer or touch
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void APVPBattlePlayerController::OnSetDestinationReleased()
{
	// If it was a short press
	if (FollowTime <= ShortPressThreshold)
	{
		// We move there and spawn some particles
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
}

// Triggered every frame when the input is held down
void APVPBattlePlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void APVPBattlePlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}
