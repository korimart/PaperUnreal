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
#include "PaperUnreal/ModeAgnostic/GameStarterComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"
#include "PaperUnreal/Widgets/BattleRuleConfigWidget.h"
#include "PaperUnreal/Widgets/SelectCharacterWidget.h"
#include "PaperUnreal/Widgets/ToastWidget.h"
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
	TSubclassOf<UToastWidget> ToastWidgetClass;
	
	UPROPERTY()
	UToastWidget* ToastWidget;
	FScopedAddToViewport ToastWidgetAddToViewport;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UBattleRuleConfigWidget> ConfigWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USelectCharacterWidget> SelectCharacterWidgetClass;
	TLiveData<bool> bSelectingCharacter{false};

	UPROPERTY(EditAnywhere)
	UInputAction* EditConfigAction;
	FSimpleMulticastDelegate OnEditConfigActionTriggered;
	void OnEditConfigActionTriggeredFunc() { OnEditConfigActionTriggered.Broadcast(); }

	UPROPERTY(EditAnywhere)
	UInputAction* StartGameAction;
	FSimpleMulticastDelegate OnStartGameActionTriggered;
	void OnStartGameActionTriggeredFunc() { OnStartGameActionTriggered.Broadcast(); }

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetEnhancedInputComponent()->BindAction(EditConfigAction, ETriggerEvent::Triggered, this, &ThisClass::OnEditConfigActionTriggeredFunc);
		GetEnhancedInputComponent()->BindAction(StartGameAction, ETriggerEvent::Triggered, this, &ThisClass::OnStartGameActionTriggeredFunc);

		ToastWidget = CreateWidget<UToastWidget>(GetOwningPlayerController(), ToastWidgetClass);
		ToastWidgetAddToViewport = ToastWidget;

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			StageComponent = (co_await WaitForGameState(GetWorld()))->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			ListenToEditConfigPrivilege();
			ListenToStarGamePrivilege();

			// TODO make this work
			// RunWeakCoroutine(this,
			// 	Awaitables::AllOf(
			// 		MakeComponentStream<UBattleRuleConfigComponent>(PC) | Awaitables::IsValid(),
			// 		StageComponent->GetCurrentStage().CreateStream() | Awaitables::Equals(PVPBattleStage::WaitingForStart),
			// 		bSelectingCharacter.CreateStream() | Awaitables::Negate())
			// 	| Awaitables::WhileTrue([this]() { return ListenToEditConfigActionTrigger(); }));

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
		co_await
		(
			Stream::Combine
			(
				MakeComponentStream<UBattleRuleConfigComponent>(GetOwningPlayerController()),
				StageComponent->GetCurrentStage().CreateStream(),
				bSelectingCharacter.CreateStream()
			)
			| Awaitables::Transform([](UBattleRuleConfigComponent* Config, FName Stage, bool bSelectingCharacter)
			{
				return IsValid(Config) && Stage == PVPBattleStage::WaitingForStart && !bSelectingCharacter;
			})
			| Awaitables::WhileTrue([this]()
			{
				return ListenToEditConfigActionTrigger();
			})
		);
	}

	FWeakCoroutine ListenToEditConfigActionTrigger()
	{
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[E]를 눌러서 방 설정을 변경할 수 있습니다.")));
		
		while (true)
		{
			co_await OnEditConfigActionTriggered;
			TAbortableCoroutineHandle EditConfigCoroutine = EditConfig();
			co_await Awaitables::AnyOf(OnEditConfigActionTriggered, EditConfigCoroutine.Get());
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

	FWeakCoroutine ListenToStarGamePrivilege()
	{
		co_await
		(
			Stream::Combine
			(
				MakeComponentStream<UGameStarterComponent>(GetOwningPlayerController()),
				StageComponent->GetCurrentStage().CreateStream(),
				bSelectingCharacter.CreateStream()
			)
			| Awaitables::Transform([](UGameStarterComponent* Starter, FName Stage, bool bSelectingCharacter)
			{
				return IsValid(Starter) && Stage == PVPBattleStage::WaitingForStart && !bSelectingCharacter;
			})
			| Awaitables::WhileTrue([this]()
			{
				return ListenToStartGameActionTrigger();
			})
		);
	}

	FWeakCoroutine ListenToStartGameActionTrigger()
	{
		auto GameStarter = co_await GetOwningPlayerController()->FindComponentByClass<UGameStarterComponent>();
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[S]를 눌러서 게임을 시작할 수 있습니다.")));

		while (true)
		{
			co_await OnStartGameActionTriggered;

			if (co_await GameStarter->StartGame())
			{
				break;
			}

			RunWeakCoroutine([&]() -> FWeakCoroutine
			{
				FToastHandle T = ToastWidget->Toast(FText::FromString(TEXT("게임 시작 조건을 충족하지 못했습니다.")));
				co_await WaitForSeconds(GetWorld(), 3.f);
			});
		}
	}

	FWeakCoroutine SelectCharacterAndShowHUD()
	{
		co_await SelectCharacter();

		{
			FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("방장이 게임을 시작하길 기다리는 중...")));
			co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Playing);
		}

		// TODO 플레이를 시작하면 플레이 HUD 띄우기
	}

	FWeakCoroutine SelectCharacter()
	{
		auto CharacterSetter = co_await WaitForComponent<UCharacterSetterComponent>(GetOwningPlayerController());
		auto ReadySetter = co_await WaitForComponent<UReadySetterComponent>(GetOwningPlayerController());

		bSelectingCharacter = true;
		auto F = FinallyIfValid(this, [this]() { bSelectingCharacter = false; });

		auto SelectCharacterWidget = co_await CreateWidget<USelectCharacterWidget>(GetOwningPlayerController(), SelectCharacterWidgetClass);
		auto S = ScopedAddToViewport(SelectCharacterWidget);

		const int32 Selection = co_await Awaitables::AnyOf(SelectCharacterWidget->OnManny, SelectCharacterWidget->OnQuinn);
		Selection == 0 ? CharacterSetter->ServerEquipManny() : CharacterSetter->ServerEquipQuinn();
		ReadySetter->ServerSetReady(true);
	}
};
