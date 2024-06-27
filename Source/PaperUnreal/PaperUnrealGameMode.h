// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaperUnrealGameMode.generated.h"

UCLASS(minimalapi)
class APaperUnrealGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	// TODO
	int32 NextTeamIndex = 0;
	
	APaperUnrealGameMode();

	virtual void BeginPlay() override;
	virtual void OnPostLogin(AController* NewPlayer) override;
};



