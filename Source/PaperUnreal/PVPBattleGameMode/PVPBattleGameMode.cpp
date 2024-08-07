// Fill out your copyright notice in the Description page of Project Settings.

#include "PVPBattleGameMode.h"

#include "PVPBattleGameState.h"
#include "PVPBattleHUD.h"
#include "PVPBattlePlayerState.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/BattleRule/BattleRuleComponent.h"
#include "PaperUnreal/BattleRule/BattleRuleConfigComponent.h"
#include "PaperUnreal/FreeRule/FreeRuleComponent.h"
#include "PaperUnreal/ModeAgnostic/FixedCameraPawn.h"
#include "PaperUnreal/ModeAgnostic/ThirdPersonTemplatePlayerController.h"
#include "UObject/ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(LogPVPBattleGameMode);


APVPBattleGameMode::APVPBattleGameMode()
{
	GameStateClass = APVPBattleGameState::StaticClass();
	PlayerControllerClass = AThirdPersonTemplatePlayerController::StaticClass();
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

	RunWeakCoroutine(this, [this]() -> FWeakCoroutine
	{
		const auto SetupComponentsForPrivileges = [](UPrivilegeComponent* PrivilegeComponent)
		{
			PrivilegeComponent->AddComponentForPrivilege(PVPBattlePrivilege::Host, UBattleRuleConfigComponent::StaticClass());
			// PrivilegeComponent->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UBattleRuleConfigComponent::StaticClass());
		};
		
		auto PrivilegeStream
			= GetGameState<APVPBattleGameState>()->GetPlayerStateArray().CreateAddStream()
			| Awaitables::Cast<APVPBattlePlayerState>()
			| Awaitables::TransformIfNotError(&APVPBattlePlayerState::ServerPrivilegeComponent);

		{
			auto FirstPlayerPrivilege = co_await PrivilegeStream;
			SetupComponentsForPrivileges(FirstPlayerPrivilege);
			FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Host);
			FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
		}

		while (true)
		{
			auto PlayerPrivilege = co_await PrivilegeStream;
			SetupComponentsForPrivileges(PlayerPrivilege);
			PlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
		}
	});

	RunWeakCoroutine(this, [this]() -> FWeakCoroutine
	{
		GetGameState<APVPBattleGameState>()->StageComponent->SetCurrentStage(PVPBattleStage::WaitingForConfig);

		{
			auto FreeRule = NewObject<UFreeRuleComponent>(this);
			FreeRule->RegisterComponent();
			FreeRule->Start(DefaultPawnClass);
			auto F = FinallyIfValid(FreeRule, [FreeRule]() { FreeRule->DestroyComponent(); });

			// TODO 방설정 완료될 때까지 대기

			auto AtLeast2Ready = GetGameState<APVPBattleGameState>()->ReadyStateTrackerComponent->ReadyCountIsAtLeast(2);
			auto ReadyByCheat = MakeFutureFromDelegate(UInGameCheats::OnStartGameByCheat);
			co_await Awaitables::AnyOf(AtLeast2Ready, ReadyByCheat);
		}

		const FBattleRuleResult GameResult = co_await [&]()
		{
			auto BattleRule = NewObject<UBattleRuleComponent>(this);
			BattleRule->RegisterComponent();
			return BattleRule->Start(DefaultPawnClass, 2, 2);
		}();

		GetGameState<APVPBattleGameState>()->StageComponent->SetCurrentStage(PVPBattleStage::Result);

		for (const auto& Each : GameResult.SortedByRank)
		{
			UE_LOG(LogPVPBattleGameMode, Log, TEXT("게임 결과 팀 : %d / 면적 : %f"), Each.TeamIndex, Each.Area);
		}

		for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
		{
			Each->GetPlayerController()->ChangeState(NAME_Spectating);
			Each->GetPlayerController()->ClientGotoState(NAME_Spectating);
		}

		// TODO replicate game result

		// TODO repeat
	});
}
