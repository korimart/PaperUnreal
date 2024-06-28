// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealCharacter.h"

#include "AreaActor.h"
#include "AreaSpawnerComponent.h"
#include "TeamComponent.h"
#include "TracerAreaExpanderComponent.h"
#include "TracerMeshComponent.h"
#include "Core/UECoroutine.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Utils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

APaperUnrealCharacter::APaperUnrealCharacter()
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

	TracerMeshComponent = CreateDefaultSubobject<UTracerMeshComponent>(TEXT("TracerMeshComponent"));
}

void APaperUnrealCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_Client)
	{
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			const auto GameState = co_await WaitForGameState(GetWorld());
			const auto AreaSpawnerComponent = co_await WaitForComponent<UAreaSpawnerComponent>(GameState);
			const auto TeamComponent = co_await WaitForComponent<UTeamComponent>(co_await WaitForPlayerState());
			const int32 TeamIndex = co_await TeamComponent->GetTeamIndex().WaitForValue(this);
			const AAreaActor* Area = co_await AreaSpawnerComponent->GetAreaFor(TeamIndex).WaitForValue(this);

			const auto AreaExpanderComponent = NewObject<UTracerAreaExpanderComponent>(this);
			AreaExpanderComponent->SetExpansionTarget(Area->AreaMeshComponent);
			AreaExpanderComponent->RegisterComponent();
			
			const auto RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);
			if (co_await RR->GetbResourcesLoaded().WaitForValue(this))
			{
				TracerMeshComponent->ConfigureMaterialSet({RR->GetTracerMaterialFor(TeamIndex)});
			}
		});
	}
}