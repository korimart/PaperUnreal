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

	const TSoftObjectPtr<UMaterialInstance> SoftSolidBlue{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue.MI_Solid_Blue'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidBlueLight{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue_Light.MI_Solid_Blue_Light'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidRed{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red.MI_Solid_Red'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidRedLight{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red_Light.MI_Solid_Red_Light'")}};
	const int32 ThisPlayerTeamIndex = NextTeamIndex++;
	
	NewPlayer->GetPlayerState<APaperUnrealPlayerState>()->TeamComponent->SetTeamIndex(ThisPlayerTeamIndex);

	AAreaActor* ThisPlayerArea =
		ValidOrNull(GetGameState<APaperUnrealGameState>()
		            ->AreaSpawnerComponent->GetSpawnedArea().GetHistory().FindByPredicate([&](AAreaActor* Each)
		            {
			            return *Each->TeamComponent->GetTeamIndex().GetValue() == ThisPlayerTeamIndex;
		            }));

	if (!ThisPlayerArea)
	{
		ThisPlayerArea = GetGameState<APaperUnrealGameState>()->AreaSpawnerComponent->SpawnAreaAtRandomEmptyLocation();
		ThisPlayerArea->TeamComponent->SetTeamIndex(ThisPlayerTeamIndex);
		ThisPlayerArea->SetAreaMaterial(SoftSolidBlue);
	}

	// 월드가 꽉차서 새 영역을 선포할 수 없음
	if (!ThisPlayerArea)
	{
		return;
	}

	NewPlayer->GetPlayerState<APaperUnrealPlayerState>()->InventoryComponent->SetHomeArea(ThisPlayerArea);
	NewPlayer->GetPlayerState<APaperUnrealPlayerState>()->InventoryComponent->SetTracerMaterial(SoftSolidBlueLight);

	RestartPlayerAtTransform(NewPlayer, ThisPlayerArea->GetActorTransform());
}
