// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperUnrealGameMode.h"
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
}

void APaperUnrealGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	const int32 ThisPlayerTeamIndex = NextTeamIndex++;
	NewPlayer->GetPlayerState<APaperUnrealPlayerState>()->TeamComponent->SetTeamIndex(ThisPlayerTeamIndex);

	AAreaActor* ThisPlayerArea =
		ValidOrNull(GetGameState<APaperUnrealGameState>()
		            ->AreaSpawnerComponent->GetSpawnedAreas().FindByPredicate([&](AAreaActor* Each)
		            {
			            return *Each->TeamComponent->GetTeamIndex().GetValue() == ThisPlayerTeamIndex;
		            }));

	if (!ThisPlayerArea)
	{
		ThisPlayerArea = GetGameState<APaperUnrealGameState>()
		                 ->AreaSpawnerComponent->SpawnAreaAtRandomEmptyLocation(ThisPlayerTeamIndex);
	}

	RestartPlayerAtTransform(NewPlayer, ThisPlayerArea->GetActorTransform());
}