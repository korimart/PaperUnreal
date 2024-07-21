// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaperUnrealGameMode.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPaperUnrealGameMode, Log, All);


UCLASS(minimalapi)
class APaperUnrealGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	APaperUnrealGameMode();

	virtual void BeginPlay() override;
};
