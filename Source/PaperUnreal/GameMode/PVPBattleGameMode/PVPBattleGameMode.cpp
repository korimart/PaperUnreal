// Fill out your copyright notice in the Description page of Project Settings.

#include "PVPBattleGameMode.h"
#include "PVPBattleHUD.h"
#include "PaperUnreal/GameMode/ModeAgnostic/FixedCameraPawn.h"
#include "UObject/ConstructorHelpers.h"


APVPBattleGameMode::APVPBattleGameMode()
{
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

	CreateDefaultSubobject<UPVPBattleGameModeComponent>(TEXT("PVPBattleGameModeComponent"));
}