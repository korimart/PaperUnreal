// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPBattleGameMode.h"

#include "PVPBattlePlayerController.h"
#include "PVPBattleGameState.h"
#include "PVPBattleHUD.h"
#include "PVPBattlePlayerState.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/BattleRule/BattleRuleComponent.h"
#include "PaperUnreal/FreeRule/FreeRuleComponent.h"
#include "PaperUnreal/ModeAgnostic/FixedCameraPawn.h"
#include "UObject/ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(LogPVPBattleGameMode);


APVPBattleGameMode::APVPBattleGameMode()
{
	GameStateClass = APVPBattleGameState::StaticClass();
	PlayerControllerClass = APVPBattlePlayerController::StaticClass();
	PlayerStateClass = APVPBattlePlayerState::StaticClass();
	SpectatorClass = AFixedCameraPawn::StaticClass();
	HUDClass = APVPBattleHUD::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// set default controller to our Blueprinted controller
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownPlayerController"));
	if (PlayerControllerBPClass.Class != NULL)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}

	bStartPlayersAsSpectators = true;
}

void APVPBattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		{
			auto FreeRule = NewObject<UFreeRuleComponent>(this);
			FreeRule->RegisterComponent();
			FreeRule->Start(DefaultPawnClass);
			auto F = FinallyIfValid(FreeRule, [FreeRule](){ FreeRule->DestroyComponent(); });

			// TODO 방설정 완료될 때까지 대기

			auto AtLeast2Ready = GetGameState<APVPBattleGameState>()->ReadyStateTrackerComponent->ReadyCountIsAtLeast(2);
			auto ReadyByCheat = MakeFutureFromDelegate(UInGameCheats::OnStartGameByCheat);
			co_await AnyOf(MoveTemp(AtLeast2Ready), MoveTemp(ReadyByCheat));
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
