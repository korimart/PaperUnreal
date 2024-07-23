// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameMode.h"
#include "GameFramework/HUD.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PVPBattleHUD.generated.h"

/**
 * 
 */
UCLASS()
class APVPBattleHUD : public AHUD
{
	GENERATED_BODY()

private:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			AGameStateBase* GameState = co_await AbortOnError(WaitForGameState(GetWorld()));
			UStageComponent* StageComponent = GameState->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			// TODO 방장 여부 기다려서 방장이면 방 설정 화면 띄우기

			// TODO 캐릭터 선택 화면 띄우기

			// TODO 플레이를 시작하면 플레이 HUD 띄우기

			co_await AbortOnError(StageComponent->GetCurrentStage().If(PVPBattleStage::Result));

			// TODO 캐릭터 선택 화면이건 플레이 HUD건 띄워져 있는 거 없애고 결과창 띄우기
			UE_LOG(LogTemp, Warning, TEXT("결과창"));

			// TODO 위에서부터 반복
		});
	}
};
