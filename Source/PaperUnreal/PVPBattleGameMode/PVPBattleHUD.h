// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameMode.h"
#include "PVPBattlePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/ModeAgnostic/PrivilegeComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PVPBattleHUD.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattleHUD : public AHUD2
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UStageComponent* StageComponent;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			StageComponent = (co_await WaitForGameState(GetWorld()))->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			InitiateHostSequence();

			// TODO 캐릭터 선택 화면 띄우기

			// TODO 플레이를 시작하면 플레이 HUD 띄우기

			co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Result);

			// TODO 캐릭터 선택 화면이건 플레이 HUD건 띄워져 있는 거 없애고 결과창 띄우기
			UE_LOG(LogTemp, Warning, TEXT("결과창"));

			// TODO 위에서부터 반복
		});
	}

	void InitiateHostSequence()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto PlayerState = co_await GetOwningPlayerState<APVPBattlePlayerState>();
			co_await PlayerState->PrivilegeComponent->GetbHost().If(true);

			// TODO open widget
		});
	}
};
