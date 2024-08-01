// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonTemplatePlayerController.h"
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
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AThirdPersonTemplatePlayerController::AThirdPersonTemplatePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void AThirdPersonTemplatePlayerController::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
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

void AThirdPersonTemplatePlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Setup mouse input events
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &AThirdPersonTemplatePlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &AThirdPersonTemplatePlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &AThirdPersonTemplatePlayerController::OnSetDestinationReleased);
		EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &AThirdPersonTemplatePlayerController::OnSetDestinationReleased);

		// Setup touch input events
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &AThirdPersonTemplatePlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &AThirdPersonTemplatePlayerController::OnTouchTriggered);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &AThirdPersonTemplatePlayerController::OnTouchReleased);
		EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &AThirdPersonTemplatePlayerController::OnTouchReleased);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AThirdPersonTemplatePlayerController::OnInputStarted()
{
	StopMovement();
}

// Triggered every frame when the input is held down
void AThirdPersonTemplatePlayerController::OnSetDestinationTriggered()
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

void AThirdPersonTemplatePlayerController::OnSetDestinationReleased()
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
void AThirdPersonTemplatePlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void AThirdPersonTemplatePlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}
