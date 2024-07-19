// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealGameMode.h"

#include "BattleGameModeComponent.h"
#include "PaperUnrealCharacter.h"
#include "PaperUnrealPlayerController.h"
#include "PaperUnrealGameState.h"
#include "PaperUnrealPlayerState.h"
#include "UObject/ConstructorHelpers.h"

APaperUnrealGameMode::APaperUnrealGameMode()
{
	GameStateClass = APaperUnrealGameState::StaticClass();

	// use our custom PlayerController class
	PlayerControllerClass = APaperUnrealPlayerController::StaticClass();

	PlayerStateClass = APaperUnrealPlayerState::StaticClass();

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

void APaperUnrealGameMode::BeginPlay()
{
	Super::BeginPlay();

	RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
	{
		// TODO 자유 행동 game mode 설정
		
		// TODO 방설정 완료될 때까지 대기
		
		co_await GetGameState<APaperUnrealGameState>()->ReadyStateTrackerComponent->WaitUntilCountIsAtLeast(1);

		// TODO 실제 game mode 설정
		auto BattleMode = NewObject<UBattleGameModeComponent>(this);
		BattleMode->SetPawnClass(DefaultPawnClass);
		BattleMode->RegisterComponent();
		
		const FBattleModeGameResult GameResult = co_await AbortOnError(BattleMode->Start(2, 2));

		// TODO replicate game result

		// TODO repeat
	});
}
