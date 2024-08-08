// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/BattleRule/BattleRuleConfigComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/ModeAgnostic/CharacterSetterComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"
#include "PaperUnreal/Widgets/BattleRuleConfigWidget.h"
#include "PaperUnreal/Widgets/SelectCharacterWidget.h"
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
	TSubclassOf<UBattleRuleConfigWidget> ConfigWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USelectCharacterWidget> SelectCharacterWidgetClass;

	UPROPERTY(EditAnywhere)
	UInputAction* EditConfigAction;
	FSimpleMulticastDelegate OnEditConfigTriggered;
	void OnEditConfigTriggeredFunc() { OnEditConfigTriggered.Broadcast(); }

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetEnhancedInputComponent()->BindAction(EditConfigAction, ETriggerEvent::Triggered, this, &ThisClass::OnEditConfigTriggeredFunc);

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			StageComponent = (co_await WaitForGameState(GetWorld()))->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			ListenToEditConfigPrivilege();

			{
				TAbortableCoroutineHandle S = SelectCharacterAndShowHUD();
				co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Result);
			}

			// TODO 결과창 구현
			UE_LOG(LogTemp, Warning, TEXT("결과창"));

			// TODO 위에서부터 반복
		});
	}

	FWeakCoroutine ListenToEditConfigPrivilege()
	{
		co_await (
			Stream::Combine(MakeComponentStream<UBattleRuleConfigComponent>(GetOwningPlayerController()), StageComponent->GetCurrentStage().CreateStream())
			| Awaitables::Transform([](UBattleRuleConfigComponent* Config, FName Stage) { return IsValid(Config) && Stage == PVPBattleStage::WaitingForConfig; })
			| Awaitables::WhileTrue([this]() { return ListenToEditConfigActionTrigger(); }));
	}

	FWeakCoroutine ListenToEditConfigActionTrigger()
	{
		while (true)
		{
			co_await OnEditConfigTriggered;
			TAbortableCoroutineHandle EditConfigCoroutine = EditConfig();
			co_await Awaitables::AnyOf(OnEditConfigTriggered, EditConfigCoroutine->ReturnValue());
		}
	}

	FWeakCoroutine EditConfig() const
	{
		auto ConfigComponent = co_await GetOwningPlayerController()->FindComponentByClass<UBattleRuleConfigComponent>();
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

	FWeakCoroutine SelectCharacterAndShowHUD() const
	{
		co_await SelectCharacter();

		{
			// TODO wait for game start
			co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Playing);
		}

		// TODO 플레이를 시작하면 플레이 HUD 띄우기
	}

	FWeakCoroutine SelectCharacter() const
	{
		auto CharacterSetter = co_await WaitForComponent<UCharacterSetterComponent>(GetOwningPlayerController());
		auto ReadySetter = co_await WaitForComponent<UReadySetterComponent>(GetOwningPlayerController());

		auto SelectCharacterWidget = co_await CreateWidget<USelectCharacterWidget>(GetOwningPlayerController(), SelectCharacterWidgetClass);
		auto S = ScopedAddToViewport(SelectCharacterWidget);
		const int32 Selection = co_await Awaitables::AnyOf(SelectCharacterWidget->OnManny, SelectCharacterWidget->OnQuinn);
		Selection == 0 ? CharacterSetter->ServerEquipManny() : CharacterSetter->ServerEquipQuinn();
		ReadySetter->ServerSetReady(true);
	}
};
