// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealCharacter.h"

#include "AreaActor.h"
#include "AreaSlasherComponent.h"
#include "AreaSpawnerComponent.h"
#include "InventoryComponent.h"
#include "LifeComponent.h"
#include "ReplicatedTracerPathComponent.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "TracerToAreaConverterComponent.h"
#include "TracerMeshGeneratorComponent.h"
#include "TracerOverlapCheckerComponent.h"
#include "Core/UECoroutine.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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

	LifeComponent = CreateDefaultSubobject<ULifeComponent>(TEXT("LifeComponent"));
}

void APaperUnrealCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		co_await LifeComponent->WaitForDeath();

		// 현재 얘네만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라
		// 알아서 작동을 중지하기 때문에 사망 상태에 들어갈 때 일단 얘네만 파괴해준다
		// (기능을 안 하는 컴포넌트들이 남아있다는 뜻임 하지만 곧 액터를 파괴할 것이므로 상관 없음)
		if (GetNetMode() != NM_Client)
		{
			FindAndDestroyComponent<UTracerPathComponent>(this);
			FindAndDestroyComponent<UReplicatedTracerPathComponent>(this);
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

		Context.AddToWeakList(AreaSpawner);
		Context.AddToWeakList(Inventory);
		Context.AddToWeakList(MyHomeArea);
		auto HomeAreaBoundary = co_await WaitForComponent<UAreaBoundaryComponent>(MyHomeArea);

		// TODO 아래 컴포넌트들은 위에 먼저 부착된 컴포넌트들에 의존함
		// 컴포넌트를 갈아끼울 때 이러한 의존성에 대한 경고를 하는 메카니즘이 존재하지 않음

		auto TracerPath = NewObject<UTracerPathComponent>(this);
		TracerPath->SetNoPathArea(HomeAreaBoundary);
		TracerPath->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewObject<UReplicatedTracerPathComponent>(this);
			TracerPathReplicator->SetTracerPathSource(TracerPath);
			TracerPathReplicator->RegisterComponent();
		}

		auto TracerToAreaConverter = NewObject<UTracerToAreaConverterComponent>(this);
		TracerToAreaConverter->SetTracer(TracerPath);
		TracerToAreaConverter->SetConversionDestination(HomeAreaBoundary);
		TracerToAreaConverter->RegisterComponent();

		auto TracerOverlapChecker = NewObject<UTracerOverlapCheckerComponent>(this);
		TracerOverlapChecker->SetTracer(TracerPath);
		TracerOverlapChecker->OnTracerBumpedInto.AddWeakLambda(this, [this](UTracerPathComponent* Bumpee)
		{
			if (auto LifeComponent = Bumpee->GetOwner()->FindComponentByClass<ULifeComponent>())
			{
				LifeComponent->SetbAlive(false);
			}
		});
		TracerOverlapChecker->RegisterComponent();

		RunWeakCoroutine(this, [
				this,
				AreaSpawner,
				MyHomeArea,
				TracerToAreaConverter
			](FWeakCoroutineContext& Context) -> FWeakCoroutine
			{
				Context.AddToWeakList(AreaSpawner);
				Context.AddToWeakList(MyHomeArea);
				Context.AddToWeakList(TracerToAreaConverter);

				for (auto SpawnedAreaStream = AreaSpawner->GetSpawnedAreaStreamer().CreateStream();;)
				{
					if (AAreaActor* Area = co_await SpawnedAreaStream.Next(); Area != MyHomeArea)
					{
						auto AreaSlasherComponent = NewObject<UAreaSlasherComponent>(this);
						AreaSlasherComponent->SetSlashTarget(Area->FindComponentByClass<UAreaBoundaryComponent>());
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
		UInventoryComponent* Inventory = Context.AddToWeakList(PlayerState->FindComponentByClass<UInventoryComponent>());
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
				Context.AddToWeakList(TracerMesh);
				while (true)
				{
					auto SoftTracerMaterial = co_await TracerMaterialStream.Next();
					TracerMesh->ConfigureMaterialSet({co_await RequestAsyncLoad(SoftTracerMaterial)});
				}
			});
	});
}
