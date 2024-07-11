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
#include "TracerMeshGeneratorComponent.h"
#include "TracerOverlapCheckerComponent.h"
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
		const int32 MyTeamIndex = co_await MyTeamComponent->GetTeamIndex().WaitForValue();
		UAreaSpawnerComponent* AreaSpawnerComponent = co_await WaitForComponent<UAreaSpawnerComponent>(GameState);

		AAreaActor* MyTeamArea = co_await FirstInStream(
			AreaSpawnerComponent->CreateSpawnedAreaStream(), [MyTeamIndex](AAreaActor* Area)
			{
				return *Area->TeamComponent->GetTeamIndex().GetValue() == MyTeamIndex;
			});

		UAreaMeshComponent* AreaMeshComponent = co_await WaitForComponent<UAreaMeshComponent>(MyTeamArea);

		UResourceRegistryComponent* RR = co_await WaitForComponent<UResourceRegistryComponent>(GameState);
		// TODO graceful exit
		check(co_await RR->GetbResourcesLoaded().WaitForValue());

		ITracerPathStream* TracerMeshSource = nullptr;
		if (GetNetMode() == NM_Client)
		{
			TracerMeshSource = co_await WaitForComponent<UReplicatedTracerPathComponent>(this);
			co_await Cast<UReplicatedTracerPathComponent>(TracerMeshSource)->WaitForClientInitComplete();
		}
		else
		{
			TracerMeshSource = co_await WaitForComponent<UTracerPathComponent>(this);
		}

		auto TracerMesh = NewObject<UTracerMeshComponent>(this);
		TracerMesh->RegisterComponent();
		TracerMesh->ConfigureMaterialSet({RR->GetTracerMaterialFor(MyTeamIndex)});

		auto TracerMeshGenerator = NewObject<UTracerMeshGeneratorComponent>(this);
		TracerMeshGenerator->SetMeshSource(TracerMeshSource);
		TracerMeshGenerator->SetMeshDestination(TracerMesh);
		TracerMeshGenerator->SetMeshAttachmentTarget(AreaMeshComponent);
		TracerMeshGenerator->RegisterComponent();
	});
}

void APaperUnrealCharacter::AttachServerMachineComponents()
{
	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		// TODO 서버에서는 기다리지 말자
		AGameStateBase* GameState = co_await WaitForGameState(GetWorld());
		UTeamComponent* MyTeamComponent = co_await WaitForComponent<UTeamComponent>(co_await WaitForPlayerState());
		const int32 MyTeamIndex = co_await MyTeamComponent->GetTeamIndex().WaitForValue();
		UAreaSpawnerComponent* AreaSpawnerComponent = co_await WaitForComponent<UAreaSpawnerComponent>(GameState);

		AAreaActor* MyTeamArea = co_await FirstInStream(
			AreaSpawnerComponent->CreateSpawnedAreaStream(), [MyTeamIndex](AAreaActor* Area)
			{
				return *Area->TeamComponent->GetTeamIndex().GetValue() == MyTeamIndex;
			});

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

		auto TracerOverlapChecker = NewObject<UTracerOverlapCheckerComponent>(this);
		TracerOverlapChecker->SetTracer(TracerPath);
		TracerOverlapChecker->RegisterComponent();

		TracerOverlapChecker->OnTracerBumpedInto.AddWeakLambda(this, [this](auto)
		{
			UE_LOG(LogTemp, Warning, TEXT("sdfiljsdoif"));
		});

		RunWeakCoroutine(this, [
				this,
				AreaSpawnerComponent,
				MyTeamArea,
				TracerToAreaConverter
			](FWeakCoroutineContext& Context) -> FWeakCoroutine
			{
				Context.AddToWeakList(AreaSpawnerComponent);
				Context.AddToWeakList(MyTeamArea);
				Context.AddToWeakList(TracerToAreaConverter);

				for (auto SpawnedAreaStream = AreaSpawnerComponent->CreateSpawnedAreaStream();;)
				{
					if (AAreaActor* Area = co_await SpawnedAreaStream.Next(); MyTeamArea != Area)
					{
						auto AreaBoundaryComponent = co_await WaitForComponent<UAreaBoundaryComponent>(Area);

						auto AreaSlasherComponent = NewObject<UAreaSlasherComponent>(this);
						AreaSlasherComponent->SetSlashTarget(AreaBoundaryComponent);
						AreaSlasherComponent->SetTracerToAreaConverter(TracerToAreaConverter);
						AreaSlasherComponent->RegisterComponent();
					}
				}
			});

		// RunWeakCoroutine(this, [
		// 		this,
		// 		TracerOverlapChecker
		// 	](FWeakCoroutineContext& Context) -> FWeakCoroutine
		// 	{
		// 		Context.AddToWeakList(TracerOverlapChecker);
		//
		// 		for (auto SpawnedPlayerCharacterStream = ;;)
		// 		{
		// 			APaperUnrealCharacter* Character = co_await SpawnedPlayerCharacterStream.Next();
		// 			auto NewTracer = Character->FindComponentByClass<UTracerPathComponent>();
		// 			TracerOverlapChecker->AddOverlapTarget(NewTracer);
		// 		}
		// 	});
	});
}
