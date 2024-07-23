// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPBattleCharacter.h"

#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/AreaTracer/AreaSlasherComponent.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/TracerKillerComponent.h"
#include "PaperUnreal/AreaTracer/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerMeshComponent.h"
#include "PaperUnreal/AreaTracer/TracerMeshGeneratorComponent.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerToAreaConverterComponent.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/ModeAgnostic/PlayerSpawnerComponent.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"

APVPBattleCharacter::APVPBattleCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}