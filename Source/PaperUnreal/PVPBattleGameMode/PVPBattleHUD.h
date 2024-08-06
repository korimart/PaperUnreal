// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/BattleRule/BattleRuleConfigComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"
#include "PaperUnreal/Widgets/BattleRuleConfigWidget.h"
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

	UPROPERTY(EditAnywhere)
	UInputAction* EditConfigAction;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UBattleRuleConfigWidget> ConfigWidgetClass;

	TAbortableCoroutineHandle<FWeakCoroutine> EditConfigCoroutine;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			StageComponent = (co_await WaitForGameState(GetWorld()))->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			RunWeakCoroutine(this, [this]() -> FWeakCoroutine
			{
				// TODO 자격이 있을 때만 도는 coroutine이 있으면 자격이 있을 때 유저에게 알려주는 등의 처리를 해줄 수 있음
				// 지금은 바인딩을 추가하고 버튼을 누른 다음에 자격이 있는지 검사하기 때문에 불가능 함
				auto GoodTimeForEditingConfig
					= StageComponent->GetCurrentStage().CreateStream()
					| Awaitables::TransformIfNotError([](FName Stage) { return Stage == PVPBattleStage::WaitingForConfig; });

				while (true)
				{
					co_await (GoodTimeForEditingConfig | Awaitables::If(true));
					auto Handle = GetEnhancedInputComponent()->BindAction(EditConfigAction, ETriggerEvent::Triggered, this, &ThisClass::OnEditConfigTriggered).GetHandle();
					co_await (GoodTimeForEditingConfig | Awaitables::If(false));
					GetEnhancedInputComponent()->RemoveBindingByHandle(Handle);
				}
			});

			// TODO 캐릭터 선택 화면 띄우기

			// TODO 플레이를 시작하면 플레이 HUD 띄우기

			co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Result);

			// TODO 캐릭터 선택 화면이건 플레이 HUD건 띄워져 있는 거 없애고 결과창 띄우기
			UE_LOG(LogTemp, Warning, TEXT("결과창"));

			// TODO 위에서부터 반복
		});
	}

	void OnEditConfigTriggered()
	{
		if (EditConfigCoroutine)
		{
			EditConfigCoroutine.Reset();
		}
		else
		{
			EditConfigCoroutine = EditConfig();
		}
	}

	FWeakCoroutine EditConfig() const
	{
		auto ConfigComponent = co_await WaitForComponent<UBattleRuleConfigComponent>(GetOwningPlayerController());
		auto ConfigWidget = co_await CreateWidget<UBattleRuleConfigWidget>(GetOwningPlayerController(), ConfigWidgetClass);
		auto S = ScopedAddToViewport(ConfigWidget);

		ConfigWidget->OnInitialConfig(co_await ConfigComponent->FetchConfig());

		while (co_await ConfigWidget->OnSubmit)
		{
			if (co_await ConfigComponent->SendConfig(ConfigWidget->GetLastSubmittedConfig()))
			{
				break;
			}
			
			ConfigWidget->OnSubmitFailed();
		}
	}
};
