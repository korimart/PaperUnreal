// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealCharacter.h"

#include "AreaActor.h"
#include "AreaSlasherComponent.h"
#include "AreaSpawnerComponent.h"
#include "ReplicatedTracerPathComponent.h"
#include "TeamComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "TracerToAreaConverterComponent.h"
#include "TracerVertexGeneratorComponent.h"
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
}

void APaperUnrealCharacter::AttachPlayerMachineComponents()
{
	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
		UTeamComponent* MyTeamComponent = co_await WaitForComponent<UTeamComponent>(co_await WaitForPlayerState());
		const int32 MyTeamIndex = co_await MyTeamComponent->GetTeamIndex().WaitForValue(this);
		UAreaSpawnerComponent* AreaSpawnerComponent = co_await WaitForComponent<UAreaSpawnerComponent>(GameState);
		AAreaActor* MyTeamArea = co_await AreaSpawnerComponent->WaitForAreaOfTeam(MyTeamIndex);
		UAreaMeshComponent* AreaMeshComponent = co_await WaitForComponent<UAreaMeshComponent>(MyTeamArea);

		UResourceRegistryComponent* RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);
		// TODO graceful exit
		check(co_await RR->GetbResourcesLoaded().WaitForValue(this));

		ITracerPathGenerator* TracerVertexSource = nullptr;
		if (GetNetMode() == NM_Client)
		{
			TracerVertexSource = co_await WaitForComponent<UReplicatedTracerPathComponent>(this);
		}
		else
		{
			TracerVertexSource = co_await WaitForComponent<UTracerPathComponent>(this);
		}

		auto TracerMesh = NewObject<UTracerMeshComponent>(this);
		TracerMesh->RegisterComponent();
		TracerMesh->ConfigureMaterialSet({RR->GetTracerMaterialFor(MyTeamIndex)});

		auto TracerVertexGenerator = NewObject<UTracerVertexGeneratorComponent>(this);
		TracerVertexGenerator->SetVertexSource(TracerVertexSource);
		TracerVertexGenerator->SetVertexDestination(TracerMesh);
		TracerVertexGenerator->SetVertexAttachmentTarget(AreaMeshComponent);
		TracerVertexGenerator->RegisterComponent();
	});
}

void APaperUnrealCharacter::AttachServerMachineComponents()
{
	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
		UTeamComponent* MyTeamComponent = co_await WaitForComponent<UTeamComponent>(co_await WaitForPlayerState());
		const int32 MyTeamIndex = co_await MyTeamComponent->GetTeamIndex().WaitForValue(this);
		UAreaSpawnerComponent* AreaSpawnerComponent = co_await WaitForComponent<UAreaSpawnerComponent>(GameState);
		AAreaActor* MyTeamArea = co_await AreaSpawnerComponent->WaitForAreaOfTeam(MyTeamIndex);
		UAreaBoundaryComponent* AreaBoundaryComponent = co_await WaitForComponent<UAreaBoundaryComponent>(MyTeamArea);

		// TODO 아래 컴포넌트들은 위에 먼저 부착된 컴포넌트들에 의존함
		// 컴포넌트를 갈아끼울 때 이러한 의존성에 대한 경고를 하는 메카니즘이 존재하지 않음

		auto TracerPath = NewObject<UTracerPathComponent>(this);
		TracerPath->SetNoPathArea(AreaBoundaryComponent);
		TracerPath->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewObject<UReplicatedTracerPathComponent>(this);
			TracerPathReplicator->SetTracerPathSource(TracerPath);
			TracerPathReplicator->RegisterComponent();
		}

		auto TracerToAreaConverter = NewObject<UTracerToAreaConverterComponent>(this);
		TracerToAreaConverter->SetTracer(TracerPath);
		TracerToAreaConverter->SetConversionDestination(AreaBoundaryComponent);
		TracerToAreaConverter->RegisterComponent();

		RunWeakCoroutine(this, [
				this,
				AreaSpawnerComponent,
				MyTeamArea,
				TracerToAreaConverter](FWeakCoroutineContext& Context) -> FWeakCoroutine
			{
				Context.AddToWeakList(AreaSpawnerComponent);
				Context.AddToWeakList(MyTeamArea);
				Context.AddToWeakList(TracerToAreaConverter);

				auto SpawnedAreaGenerator = AreaSpawnerComponent->CreateSpawnedAreaGenerator();

				while (true)
				{
					if (AAreaActor* Area = co_await SpawnedAreaGenerator.Next(); MyTeamArea != Area)
					{
						auto AreaBoundaryComponent = co_await WaitForComponent<UAreaBoundaryComponent>(Area);
						
						auto AreaSlasherComponent = NewObject<UAreaSlasherComponent>(this);
						AreaSlasherComponent->SetSlashTarget(AreaBoundaryComponent);
						AreaSlasherComponent->SetTracerToAreaConverter(TracerToAreaConverter);
						AreaSlasherComponent->RegisterComponent();
					}
				}
			});
	});
}
