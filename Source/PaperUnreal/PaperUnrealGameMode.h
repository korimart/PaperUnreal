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
	APaperUnrealGameMode();

	virtual void BeginPlay() override;
};
