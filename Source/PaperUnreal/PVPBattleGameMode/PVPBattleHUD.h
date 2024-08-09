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

	UPROPERTY(EditAnywhere)
	TSubclassOf<UBattleRuleConfigWidget> ConfigWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USelectCharacterWidget> SelectCharacterWidgetClass;

	UPROPERTY(EditAnywhere)
	UInputAction* EditConfigAction;
	FSimpleMulticastDelegate OnEditConfigActionTriggered;
	void OnEditConfigActionTriggeredFunc() { OnEditConfigActionTriggered.Broadcast(); }

	UPROPERTY(EditAnywhere)
	UInputAction* StartGameAction;
	FSimpleMulticastDelegate OnStartGameActionTriggered;
	void OnStartGameActionTriggeredFunc() { OnStartGameActionTriggered.Broadcast(); }

	UPROPERTY(EditAnywhere)
	UInputAction* SelectCharacterAction;
	FSimpleMulticastDelegate OnSelectCharacterActionTriggered;
	void OnSelectCharacterActionTriggeredFunc() { OnSelectCharacterActionTriggered.Broadcast(); }

	TAbortableCoroutineHandle<FWeakCoroutine> OpenDialog;
	TLiveData<bool> bDialogOpen;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetEnhancedInputComponent()->BindAction(EditConfigAction, ETriggerEvent::Triggered, this, &ThisClass::OnEditConfigActionTriggeredFunc);
		GetEnhancedInputComponent()->BindAction(StartGameAction, ETriggerEvent::Triggered, this, &ThisClass::OnStartGameActionTriggeredFunc);
		GetEnhancedInputComponent()->BindAction(SelectCharacterAction, ETriggerEvent::Triggered, this, &ThisClass::OnSelectCharacterActionTriggeredFunc);

		ToastWidget = CreateWidget<UToastWidget>(GetOwningPlayerController(), ToastWidgetClass);

		RunWeakCoroutine(this,
			bDialogOpen.CreateStream()
			| Awaitables::Negate()
			| Awaitables::WhileTrue([this]() { return ShowToastWidget(); }));

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			StageComponent = (co_await WaitForGameState(GetWorld()))->FindComponentByClass<UStageComponent>();
			check(IsValid(StageComponent));

			{
				TAbortableCoroutineHandle S = SelectCharacterAndShowHUD();
				co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Result);
			}

			{
				// TODO 결과창 구현
				UE_LOG(LogTemp, Warning, TEXT("결과창"));
			}

			// TODO 위에서부터 반복
		});
	}

	FWeakCoroutine ShowToastWidget() const
	{
		FScopedAddToViewport S = ScopedAddToViewport(ToastWidget);
		co_await Awaitables::Forever();
	}

	FWeakCoroutine SelectCharacterAndShowHUD()
	{
		{
			co_await SelectCharacter();

			APlayerController* PC = GetOwningPlayerController();

			TArray<TAbortableCoroutineHandle<FWeakCoroutine>> Handles;

			Handles << RunWeakCoroutine(this,
				Stream::AllOf(
					MakeComponentStream<UBattleRuleConfigComponent>(PC) | Awaitables::IsValid(),
					bDialogOpen.CreateStream() | Awaitables::Negate())
				| Awaitables::WhileTrue([this]() { return ListenToEditConfigActionTrigger(); }));

			Handles << RunWeakCoroutine(this,
				Stream::AllOf(
					MakeComponentStream<UGameStarterComponent>(PC) | Awaitables::IsValid(),
					bDialogOpen.CreateStream() | Awaitables::Negate())
				| Awaitables::WhileTrue([this]() { return ListenToStartGameActionTrigger(); }));

			Handles << RunWeakCoroutine(this,
				Stream::AllOf(
					MakeComponentStream<UCharacterSetterComponent>(PC) | Awaitables::IsValid(),
					bDialogOpen.CreateStream() | Awaitables::Negate())
				| Awaitables::WhileTrue([this]() { return ListenToSelectCharacterActionTrigger(); }));

			FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("방장이 게임을 시작하길 기다리는 중...")));

			co_await StageComponent->GetCurrentStage().If(PVPBattleStage::Playing);
			OpenDialog.Reset();
		}

		// TODO 플레이를 시작하면 플레이 HUD 띄우기
	}

	FWeakCoroutine ListenToEditConfigActionTrigger()
	{
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[E]를 눌러서 방 설정을 변경할 수 있습니다.")), 99);

		co_await OnEditConfigActionTriggered;

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			OpenDialog = EditConfig();
			co_await Awaitables::AnyOf(OnEditConfigActionTriggered, OpenDialog.Get());
			OpenDialog.Reset();
		});
	}

	FWeakCoroutine ListenToStartGameActionTrigger()
	{
		auto GameStarter = co_await GetOwningPlayerController()->FindComponentByClass<UGameStarterComponent>();
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[S]를 눌러서 게임을 시작할 수 있습니다.")), 98);

		while (true)
		{
			co_await OnStartGameActionTriggered;

			if (co_await GameStarter->StartGame())
			{
				break;
			}

			RunWeakCoroutine([&]() -> FWeakCoroutine
			{
				FToastHandle T = ToastWidget->Toast(FText::FromString(TEXT("게임 시작 조건을 충족하지 못했습니다.")), -1);
				co_await WaitForSeconds(GetWorld(), 3.f);
			});
		}
	}

	FWeakCoroutine ListenToSelectCharacterActionTrigger()
	{
		auto CharacterSetter = co_await GetOwningPlayerController()->FindComponentByClass<UCharacterSetterComponent>();
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[H]를 눌러서 캐릭터를 변경할 수 있습니다.")), 97);

		co_await OnSelectCharacterActionTriggered;

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			OpenDialog = SelectCharacter();
			co_await Awaitables::AnyOf(OnSelectCharacterActionTriggered, OpenDialog.Get());
			OpenDialog.Reset();
		});
	}

	FWeakCoroutine EditConfig()
	{
		auto ConfigComponent = co_await GetOwningPlayerController()->FindComponentByClass<UBattleRuleConfigComponent>();
		auto ConfigWidget = co_await CreateWidget<UBattleRuleConfigWidget>(GetOwningPlayerController(), ConfigWidgetClass);
		auto S = ScopedAddToViewport(ConfigWidget);
		bDialogOpen = true;
		auto F = FinallyIfValid(this, [this]() { bDialogOpen = false; });

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

	FWeakCoroutine SelectCharacter()
	{
		auto CharacterSetter = co_await WaitForComponent<UCharacterSetterComponent>(GetOwningPlayerController());
		auto ReadySetter = co_await WaitForComponent<UReadySetterComponent>(GetOwningPlayerController());

		auto SelectCharacterWidget = co_await CreateWidget<USelectCharacterWidget>(GetOwningPlayerController(), SelectCharacterWidgetClass);
		auto S = ScopedAddToViewport(SelectCharacterWidget);
		bDialogOpen = true;
		auto F = FinallyIfValid(this, [this]() { bDialogOpen = false; });

		const int32 Selection = co_await Awaitables::AnyOf(SelectCharacterWidget->OnManny, SelectCharacterWidget->OnQuinn);
		Selection == 0 ? CharacterSetter->ServerEquipManny() : CharacterSetter->ServerEquipQuinn();
		ReadySetter->ServerSetReady(true);
	}
};
