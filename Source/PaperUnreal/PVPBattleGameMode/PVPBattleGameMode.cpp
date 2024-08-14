// Fill out your copyright notice in the Description page of Project Settings.

#include "PVPBattleGameMode.h"

#include "PVPBattleGameState.h"
#include "PVPBattleHUD.h"
#include "PVPBattlePlayerController.h"
#include "PVPBattlePlayerState.h"
#include "PaperUnreal/BattleGameMode/BattleGameModeComponent.h"
#include "PaperUnreal/FreeGameMode/FreeGameModeComponent.h"
#include "PaperUnreal/ModeAgnostic/FixedCameraPawn.h"
#include "PaperUnreal/ModeAgnostic/GameStarterComponent.h"
#include "UObject/ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(LogPVPBattleGameMode);


bool APVPBattleGameMode::StartGameIfConditionsMet()
{
	const int32 ReadyCount = Algo::TransformAccumulate(
		::GetComponents<UReadyStateComponent>(GetGameState<APVPBattleGameState>()->GetPlayerStateArray().Get()),
		[](auto Each) { return Each->GetbReady().Get() ? 1 : 0; },
		0);

	if (ReadyCount >= 2)
	{
		OnGameStartConditionsMet.Broadcast();
		return true;
	}

	return false;
}

APVPBattleGameMode::APVPBattleGameMode()
{
	GameStateClass = APVPBattleGameState::StaticClass();
	PlayerStateClass = APVPBattlePlayerState::StaticClass();
	SpectatorClass = AFixedCameraPawn::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownPlayerController"));
	if (PlayerControllerBPClass.Class != nullptr)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APVPBattleHUD> HUDBPClass(TEXT("/Game/TopDown/Blueprints/BP_PVPBattleHUD"));
	if (HUDBPClass.Class != nullptr)
	{
		HUDClass = HUDBPClass.Class;
	}

	bStartPlayersAsSpectators = true;
}

void APVPBattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitPrivilegeComponentOfNewPlayers();
	InitiateGameFlow();
}

FWeakCoroutine APVPBattleGameMode::InitPrivilegeComponentOfNewPlayers()
{
	auto AddPrivilegeComponents = [this](UPrivilegeComponent* Target)
	{
		Target->AddComponentForPrivilege(PVPBattlePrivilege::Host, UBattleConfigComponent::StaticClass());
		Target->AddComponentForPrivilege(PVPBattlePrivilege::Host, UGameStarterComponent::StaticClass(),
			FComponentInitializer::CreateWeakLambda(this, [this](UActorComponent* Component)
			{
				Cast<UGameStarterComponent>(Component)
					->ServerGameStarter.BindUObject(this, &ThisClass::StartGameIfConditionsMet);
			}));
		Target->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UCharacterSetterComponent::StaticClass());
		Target->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UReadySetterComponent::StaticClass());
		return Target;
	};

	auto PrivilegeStream
		= GetGameState<APVPBattleGameState>()->GetPlayerStateArray().CreateAddStream()
		| Awaitables::Transform(&APVPBattlePlayerState::GetPlayerController)
		| Awaitables::CastChecked<APVPBattlePlayerController>()
		| Awaitables::Transform(&APVPBattlePlayerController::ServerPrivilegeComponent)
		| Awaitables::Transform([&](UPrivilegeComponent* Component) { return AddPrivilegeComponents(Component); });

	{
		auto FirstPlayerPrivilege = co_await PrivilegeStream;
		FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Host);
		FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
	}

	while (true)
	{
		auto PlayerPrivilege = co_await PrivilegeStream;
		PlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
	}
}

FWeakCoroutine APVPBattleGameMode::InitiateGameFlow()
{
	auto StageComponent = co_await GetGameState<APVPBattleGameState>()->StageComponent;

	StageComponent->SetCurrentStage(PVPBattleStage::WaitingForStart);

	{
		auto FreeGameMode = NewObject<UFreeGameModeComponent>(this);
		FreeGameMode->RegisterComponent();
		FreeGameMode->Start(DefaultPawnClass);
		auto F = FinallyIfValid(FreeGameMode, [FreeGameMode]() { FreeGameMode->DestroyComponent(); });

		co_await OnGameStartConditionsMet;
		
		StageComponent->SetCurrentStage(PVPBattleStage::WillPlay);

		const float PlayStartTime = GetWorld()->GetTimeSeconds() + 5.f;
		StageComponent->SetStageWorldStartTime(PVPBattleStage::Playing, PlayStartTime);

		co_await GetGameState<APVPBattleGameState>()->WorldTimerComponent->At(PlayStartTime);
		StageComponent->SetCurrentStage(PVPBattleStage::Playing);
	}

	auto ResultComponent = co_await [&]()
	{
		auto BattleGameMode = NewObject<UBattleGameModeComponent>(this);
		BattleGameMode->RegisterComponent();
		return BattleGameMode->Start(DefaultPawnClass, 2, 2);
	}();

	StageComponent->SetCurrentStage(PVPBattleStage::Result);
	StageComponent->SetStageWorldStartTime(PVPBattleStage::Result, GetWorld()->GetTimeSeconds());

	for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
	{
		Each->GetPlayerController()->ChangeState(NAME_Spectating);
		Each->GetPlayerController()->ClientGotoState(NAME_Spectating);
	}

	// TODO 클라이언트들이 result를 보기에 충분한 시간을 준다
	// ResultComponent->DestroyComponent();
}
