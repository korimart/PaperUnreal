// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PVPBattleGameMode.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPVPBattleGameMode, Log, All);


UCLASS(minimalapi)
class APVPBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	APVPBattleGameMode();

	virtual void BeginPlay() override;
};
