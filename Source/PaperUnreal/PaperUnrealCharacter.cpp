// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealCharacter.h"

#include "AreaActor.h"
#include "AreaSlasherComponent.h"
#include "AreaSpawnerComponent.h"
#include "InventoryComponent.h"
#include "LifeComponent.h"
#include "PlayerKillerComponent.h"
#include "PlayerSpawnerComponent.h"
#include "ReplicatedTracerPathComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "TracerToAreaConverterComponent.h"
#include "TracerMeshGeneratorComponent.h"
#include "TracerOverlapCheckerComponent.h"
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
#include "GameFramework/GameStateBase.h"

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

	LifeComponent = CreateDefaultSubobject<ULifeComponent>(TEXT("LifeComponent"));
}

void APaperUnrealCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		co_await LifeComponent->WaitForDeath();

		// 현재 얘만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라 알아서 사라짐
		// Path를 파괴해서 상호작용을 없애 게임에 영향을 미치지 않게 한다
		if (GetNetMode() != NM_Client)
		{
			FindAndDestroyComponent<UTracerPathComponent>(this);
		}
		else
		{
			// TODO
			// PlayDeathAnimation();
		}
	});
}

void APaperUnrealCharacter::AttachServerMachineComponents()
{
	RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
	{
		APlayerState* PlayerState = co_await WaitForPlayerState();
		if (!IsValid(PlayerState))
		{
			co_return;
		}

		// 디펜던시: UAreaSpawnerComponent를 가지는 GameState에 대해서만 이 클래스를 사용할 수 있음
		UAreaSpawnerComponent* AreaSpawner = GetWorld()->GetGameState()->FindComponentByClass<UAreaSpawnerComponent>();
		if (!ensureAlways(IsValid(AreaSpawner)))
		{
			co_return;
		}

		// 디펜던시: UPlayerSpawnerComponent를 가지는 GameState에 대해서만 이 클래스를 사용할 수 있음
		UPlayerSpawnerComponent* PlayerSpawner = GetWorld()->GetGameState()->FindComponentByClass<UPlayerSpawnerComponent>();
		if (!ensureAlways(IsValid(PlayerSpawner)))
		{
			co_return;
		}

		// 디펜던시: UInventoryComponent를 가지는 PlayerState에 대해서만 이 클래스를 사용할 수 있음
		UInventoryComponent* Inventory = PlayerState->FindComponentByClass<UInventoryComponent>();
		if (!ensureAlways(IsValid(Inventory)))
		{
			co_return;
		}

		// 캐릭터의 possess 시점에 영역이 이미 준비되어 있다고 가정함
		// 영역 스폰을 기다리는 기획이 되면 기다리는 동안 뭘 할 것인지에 대한 기획이 필요함
		AAreaActor* MyHomeArea = Inventory->GetHomeArea().GetValue();
		if (!ensureAlways(IsValid(MyHomeArea)))
		{
			co_return;
		}

		Context.AbortIfNotValid(AreaSpawner);
		Context.AbortIfNotValid(PlayerSpawner);
		Context.AbortIfNotValid(Inventory);
		Context.AbortIfNotValid(MyHomeArea);
		auto HomeAreaBoundary = co_await WaitForComponent<UAreaBoundaryComponent>(MyHomeArea);

		auto TracerPath = NewObject<UTracerPathComponent>(this);
		TracerPath->SetNoPathArea(HomeAreaBoundary);
		TracerPath->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewObject<UReplicatedTracerPathComponent>(this);
			TracerPathReplicator->SetTracerPathSource(TracerPath);
			TracerPathReplicator->RegisterComponent();
		}

		auto TracerOverlapChecker = NewObject<UTracerOverlapCheckerComponent>(this);
		TracerOverlapChecker->SetTracer(TracerPath);
		TracerOverlapChecker->RegisterComponent();

		auto Killer = NewObject<UPlayerKillerComponent>(this);
		Killer->SetTracer(TracerPath);
		Killer->SetArea(HomeAreaBoundary);
		Killer->SetOverlapChecker(TracerOverlapChecker);
		Killer->RegisterComponent();

		auto AreaConverter = NewObject<UTracerToAreaConverterComponent>(this);
		AreaConverter->SetTracer(TracerPath);
		AreaConverter->SetConversionDestination(HomeAreaBoundary);
		AreaConverter->RegisterComponent();

		AreaSpawner->GetSpawnedAreaStreamer().Observe(this, [this, AS_WEAK(MyHomeArea), AS_WEAK(AreaConverter)](AAreaActor* NewArea)
		{
			if (AllValid(MyHomeArea, AreaConverter) && NewArea != MyHomeArea)
			{
				RunWeakCoroutine(this, [this, NewArea, AreaConverter](FWeakCoroutineContext& Context) -> FWeakCoroutine
				{
					Context.AbortIfNotValid(AreaConverter);

					auto NewBoundary = co_await WaitForComponent<UAreaBoundaryComponent>(NewArea);
					auto AreaSlasher = NewObject<UAreaSlasherComponent>(this);
					AreaSlasher->SetSlashTarget(NewBoundary);
					AreaSlasher->SetTracerToAreaConverter(AreaConverter.Get());
					AreaSlasher->RegisterComponent();
				});
			}
		});

		PlayerSpawner->GetSpawnedPlayerStreamer().Observe(this, [this, AS_WEAK(TracerOverlapChecker)](AActor* NewPlayer)
		{
			if (AllValid(TracerOverlapChecker))
			{
				RunWeakCoroutine(this, [this, NewPlayer, TracerOverlapChecker](FWeakCoroutineContext& Context) -> FWeakCoroutine
				{
					Context.AbortIfNotValid(TracerOverlapChecker);

					auto NewTracer = co_await WaitForComponent<UTracerPathComponent>(NewPlayer);
					TracerOverlapChecker->AddOverlapTarget(NewTracer);
				});
			}
		});
	});
}

void APaperUnrealCharacter::AttachPlayerMachineComponents()
{
	RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
	{
		APlayerState* PlayerState = co_await WaitForPlayerState();
		if (!IsValid(PlayerState))
		{
			co_return;
		}

		// 디펜던시: UInventoryComponent를 가지는 PlayerState에 대해서만 이 클래스를 사용할 수 있음
		UInventoryComponent* Inventory = Context.AbortIfNotValid(PlayerState->FindComponentByClass<UInventoryComponent>());
		if (!ensureAlways(IsValid(Inventory)))
		{
			co_return;
		}

		AAreaActor* MyHomeArea = co_await Inventory->GetHomeArea().WaitForValue();
		auto MyHomeAreaMesh = co_await WaitForComponent<UAreaMeshComponent>(MyHomeArea);

		ITracerPathStream* TracerMeshSource = GetNetMode() == NM_Client
			? static_cast<ITracerPathStream*>(FindComponentByClass<UReplicatedTracerPathComponent>())
			: static_cast<ITracerPathStream*>(FindComponentByClass<UTracerPathComponent>());

		// 클래스 서버 코드에서 뭔가 실수한 거임 고쳐야 됨
		check(TracerMeshSource);

		auto TracerMesh = NewObject<UTracerMeshComponent>(this);
		TracerMesh->RegisterComponent();

		auto TracerMeshGenerator = NewObject<UTracerMeshGeneratorComponent>(this);
		TracerMeshGenerator->SetMeshSource(TracerMeshSource);
		TracerMeshGenerator->SetMeshDestination(TracerMesh);
		TracerMeshGenerator->SetMeshAttachmentTarget(MyHomeAreaMesh);
		TracerMeshGenerator->RegisterComponent();

		// 일단 위에까지 완료했으면 플레이는 가능한 거임 여기부터는 미적인 요소들을 준비한다
		RunWeakCoroutine(this, [
				this,
				TracerMesh,
				TracerMaterialStream = Inventory->GetTracerMaterial().CreateStream()
			](FWeakCoroutineContext& Context) mutable -> FWeakCoroutine
			{
				Context.AbortIfNotValid(TracerMesh);

				while (true)
				{
					auto SoftTracerMaterial = co_await TracerMaterialStream.Next();
					TracerMesh->ConfigureMaterialSet({co_await RequestAsyncLoad(SoftTracerMaterial)});
				}
			});
	});
}
