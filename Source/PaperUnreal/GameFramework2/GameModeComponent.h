// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ComponentGroupComponent.h"
#include "GameStateBase2.h"
#include "GameModeComponent.generated.h"


UCLASS(Within=GameModeBase)
class UGameModeComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	AGameStateBase2* GetGameState() const
	{
		return GetOuterAGameModeBase()->GetGameState<AGameStateBase2>();
	}

	TSubclassOf<APawn> GetDefaultPawnClass() const
	{
		return GetOuterAGameModeBase()->DefaultPawnClass;
	}

protected:
	virtual void AttachPlayerControllerComponents(APlayerController* PC) {}
	virtual void AttachPlayerStateComponents(APlayerState* PS) {}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetGameState()->GetPlayerStateArray().ObserveAddIfValid(this, [this](APlayerState* PS)
		{
			AttachPlayerControllerComponents(PS->GetPlayerController());
			AttachPlayerStateComponents(PS);
		});
	}
};
