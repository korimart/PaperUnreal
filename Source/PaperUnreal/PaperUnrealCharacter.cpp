// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealCharacter.h"

#include "AreaActor.h"
#include "AreaSlasherComponent.h"
#include "AreaSpawnerComponent.h"
#include "PlayerCollisionStateComponent.h"
#include "TeamComponent.h"
#include "OffAreaTracerGenEnablerComponent.h"
#include "TracerMeshComponent.h"
#include "TracerToAreaConverterComponent.h"
#include "TracerVertexAttacherComponent.h"
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

		UResourceRegistryComponent* RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);
		if (!co_await RR->GetbResourcesLoaded().WaitForValue(this))
		{
			co_return;
		}

		auto TracerMesh = NewObject<UTracerMeshComponent>(this);
		TracerMesh->RegisterComponent();
		TracerMesh->ConfigureMaterialSet({RR->GetTracerMaterialFor(MyTeamIndex)});
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
		AAreaActor* MyTeamArea = co_await AreaSpawnerComponent->GetAreaFor(MyTeamIndex).WaitForValue(this);
		UAreaMeshComponent* AreaMeshComponent = MyTeamArea->AreaMeshComponent;

		// TODO 머신의 종류에 따라 다른 인터페이스를 가져온다 (싱글 및 호스트는 레플리케이션을 건너뛸 수 있도록)
		UTracerMeshComponent* TracerVertexDestination = co_await WaitForComponent<UTracerMeshComponent>(this);


		// TODO 아래 컴포넌트들은 위에 먼저 부착된 컴포넌트들에 의존함
		// 컴포넌트를 갈아끼울 때 이러한 의존성에 대한 경고를 하는 메카니즘이 존재하지 않음

		auto TracerVertexGenerator = NewObject<UTracerVertexGeneratorComponent>(this);
		TracerVertexGenerator->SetGenDestination(TracerVertexDestination);
		TracerVertexGenerator->RegisterComponent();

		auto TracerVertexModifier = NewObject<UTracerVertexAttacherComponent>(this);
		TracerVertexModifier->SetVertexGenerator(TracerVertexGenerator);
		TracerVertexModifier->SetAttachDestination(AreaMeshComponent);
		TracerVertexModifier->RegisterComponent();

		auto CollisionState = NewObject<UPlayerCollisionStateComponent>(this);
		CollisionState->RegisterComponent();

		auto TracerGenController = NewObject<UOffAreaTracerGenEnablerComponent>(this);
		TracerGenController->SetVertexGenerator(TracerVertexGenerator);
		TracerGenController->SetGenPreventionArea(AreaMeshComponent);
		TracerGenController->SetCollisionState(CollisionState);
		TracerGenController->RegisterComponent();

		auto TracerToAreaConverter = NewObject<UTracerToAreaConverterComponent>(this);
		TracerToAreaConverter->SetTracerGenController(TracerGenController);
		TracerToAreaConverter->SetConversionDestination(AreaMeshComponent);
		TracerToAreaConverter->RegisterComponent();

		AreaSpawnerComponent->GetSpawnedAreas().ObserveEach(this, [this,
				CollisionState = ToWeak(CollisionState),
				MyTeamArea = ToWeak(MyTeamArea),
				TracerToAreaConverter = ToWeak(TracerToAreaConverter)
			](AAreaActor* SpawnedArea)
			{
				if (AllValid(CollisionState, MyTeamArea, TracerToAreaConverter) && SpawnedArea != MyTeamArea)
				{
					auto AreaSlasherComponent = NewObject<UAreaSlasherComponent>(this);
					AreaSlasherComponent->SetCollisionState(CollisionState.Get());
					AreaSlasherComponent->SetSlashTarget(SpawnedArea->AreaMeshComponent);
					AreaSlasherComponent->SetTracerToAreaConverter(TracerToAreaConverter.Get());
					AreaSlasherComponent->RegisterComponent();
				}
			});
	});
}
