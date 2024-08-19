// Fill out your copyright notice in the Description page of Project Settings.


#include "MenuGameMode.h"

#include "MenuHUD.h"
#include "PaperUnreal/GameMode/ModeAgnostic/FixedCameraPawn.h"

AMenuGameMode::AMenuGameMode()
{
	SpectatorClass = AFixedCameraPawn::StaticClass();
	PlayerControllerClass = APlayerController2::StaticClass();

	static ConstructorHelpers::FClassFinder<AMenuHUD> HUDBPClass{TEXT("/Game/TopDown/Blueprints/BP_MenuHUD")};
	if (HUDBPClass.Class != nullptr)
	{
		HUDClass = HUDBPClass.Class;
	}

	bStartPlayersAsSpectators = true;
}
