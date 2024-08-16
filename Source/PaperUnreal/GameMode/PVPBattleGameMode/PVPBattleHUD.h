// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameModeComponent.h"
#include "PVPBattleGameStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleGameModeComponent.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleConfigComponent.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattlePawnComponent.h"
#include "PaperUnreal/GameFramework2/HUD2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterSetterComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/GameMode/ModeAgnostic/GameStarterComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/StageComponent.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PaperUnreal/WeakCoroutine/WhileTrueAwaitable.h"
#include "PaperUnreal/Widgets/BattleConfigWidget.h"
#include "PaperUnreal/Widgets/BattleResultWidget.h"
#include "PaperUnreal/Widgets/TeamScoresWidget.h"
#include "PaperUnreal/Widgets/SelectCharacterWidget.h"
#include "PaperUnreal/Widgets/TimeWidget.h"
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
	UWorldTimerComponent* WorldTimerComponent;

	UPROPERTY()
	UPVPBattleGameStateComponent* GameStateComponent;

	UPROPERTY()
	UStageComponent* StageComponent;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UToastWidget> ToastWidgetClass;

	UPROPERTY()
	UToastWidget* ToastWidget;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UBattleConfigWidget> ConfigWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USelectCharacterWidget> SelectCharacterWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UTeamScoresWidget> TeamScoresWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UTimeWidget> TimeWidgetClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UBattleResultWidget> ResultWidgetClass;

	UPROPERTY(EditAnywhere)
	UInputMappingContext* InGameSettingsMappingContext;

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

	TLiveData<bool> bDialogOpen;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetEnhancedInputSubsystem()->AddMappingContext(InGameSettingsMappingContext, 0);
		GetEnhancedInputComponent()->BindAction(EditConfigAction, ETriggerEvent::Triggered, this, &ThisClass::OnEditConfigActionTriggeredFunc);
		GetEnhancedInputComponent()->BindAction(StartGameAction, ETriggerEvent::Triggered, this, &ThisClass::OnStartGameActionTriggeredFunc);
		GetEnhancedInputComponent()->BindAction(SelectCharacterAction, ETriggerEvent::Triggered, this, &ThisClass::OnSelectCharacterActionTriggeredFunc);

		WorldTimerComponent = NewObject<UWorldTimerComponent>(GetWorld()->GetGameState());
		WorldTimerComponent->RegisterComponent();

		ToastWidget = CreateWidget<UToastWidget>(GetOwningPlayerController(), ToastWidgetClass);
		bDialogOpen.Observe(this, [this](bool bOpen)
		{
			bOpen ? RemoveFromParentIfHasParent(ToastWidget) : AddToViewportIfNotAdded(ToastWidget);
		});

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto GameState = co_await WaitForGameState(GetWorld());

			auto GameStateComponentStream
				= MakeComponentStream<UPVPBattleGameStateComponent>(GameState)
				// 결과창을 오래 동안 보고 있으면 여러 매치가 끝나면서
				// 스트림 안에는 이전 매치의 쓰레기가 된 Component들이 들어있음
				// 그것들은 무시하고 항상 가장 최신의 Component를 받는다
				| Awaitables::IgnoreError<UInvalidObjectError>();

			while (true)
			{
				GameStateComponent = (co_await GameStateComponentStream).Unsafe();
				StageComponent = (co_await GameStateComponent->GetStageComponent()).Unsafe();

				if (StageComponent->GetCurrentStage().Get() != PVPBattleStage::Result)
				{
					TAbortableCoroutineHandle S = InitiateMatchSequence();

					// 렉이 심하게 걸렸다가 회복된 경우 StageComponent는 이미 파괴되어
					// PVPBattleStage::Result의 Replication이 누락될 수 있음
					// 그래서 OnDestroyed도 함께 체크해준다 물론 이 경우 결과창에는 아무것도 표시할 수 없음
					// @see ThisClass::ShowResults
					co_await Awaitables::AnyOf(
						StageComponent->GetCurrentStage().If(PVPBattleStage::Result),
						StageComponent->OnDestroyed);
				}

				co_await ShowResults();
			}
		});
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		WorldTimerComponent->DestroyComponent();

		// 월드 종료로 인한 EndPlay 시에는 존재하지 않을 수 있음
		if (const auto InputSubsystem = GetEnhancedInputSubsystem())
		{
			InputSubsystem->RemoveMappingContext(InGameSettingsMappingContext);
		}
	}

	/**
	 * 이 함수는 코루틴이 살아있는 동안에 GameStateComponent와 StageComponent 멤버가 살아있다고 가정함
	 * 이 함수의 Caller가 두 컴포넌트의 Valid 함을 보장해야 함
	 */
	FWeakCoroutine InitiateMatchSequence()
	{
		co_await SelectCharacter();

		if (StageComponent->GetCurrentStage().Get() == PVPBattleStage::WaitingForStart)
		{
			FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("방장이 게임을 시작하길 기다리는 중...")));

			TArray<TAbortableCoroutineHandle<FWeakCoroutine>> Handles;
			Handles << WhileComponentIsValid<UBattleConfigComponent>(&ThisClass::ListenToEditConfigActionTrigger);
			Handles << WhileComponentIsValid<UGameStarterComponent>(&ThisClass::ListenToStartGameActionTrigger);
			Handles << WhileComponentIsValid<UCharacterSetterComponent>(&ThisClass::ListenToSelectCharacterActionTrigger);

			co_await StageComponent->GetCurrentStage().IfNot(PVPBattleStage::WaitingForStart);
		}

		if (StageComponent->GetCurrentStage().Get() == PVPBattleStage::WillPlay)
		{
			const float StartTime = co_await StageComponent->GetStageWorldStartTime(PVPBattleStage::Playing);

			TOptional CountDownToast = ToastWidget->Toast(FText::FromString(TEXT("게임 시작을 준비하는 중...")));
			co_await WorldTimerComponent->At(StartTime - 3);
			CountDownToast = ToastWidget->Toast(FText::FromString(TEXT("3초 뒤에 게임이 시작됩니다.")));
			co_await WorldTimerComponent->At(StartTime - 2);
			CountDownToast = ToastWidget->Toast(FText::FromString(TEXT("2초 뒤에 게임이 시작됩니다.")));
			co_await WorldTimerComponent->At(StartTime - 1);
			CountDownToast = ToastWidget->Toast(FText::FromString(TEXT("1초 뒤에 게임이 시작됩니다.")));

			co_await StageComponent->GetCurrentStage().IfNot(PVPBattleStage::WillPlay);
		}

		TAbortableCoroutineHandle TeamScores = ShowTeamScores();
		TAbortableCoroutineHandle Time = ShowTime();

		TAbortableCoroutineHandle<FWeakCoroutine> PawnSequence;
		auto PawnStream = GetOwningPawnOrSpectator2() | Awaitables::IfValid();
		while (true)
		{
			APawn* Pawn = (co_await PawnStream).Unsafe();
			PawnSequence = Pawn->IsA<ASpectatorPawn>()
				? InitiateSpectatorPawnSequence(Pawn)
				: InitiateBattlePawnSequence(Pawn);
		}
	}

	FWeakCoroutine InitiateSpectatorPawnSequence(APawn* SpectatorPawn)
	{
		{
			FToastHandle H = ToastWidget->Toast(FText::FromString(TEXT("경기 정원이 가득차 관전자로 배정되었습니다.")));
			co_await WaitForSeconds(GetWorld(), 5.f);
		}
		
		FToastHandle H = ToastWidget->Toast(FText::FromString(TEXT("관전 모드")));
		co_await Awaitables::Forever();
	}

	FWeakCoroutine InitiateBattlePawnSequence(APawn* BattlePawn)
	{
		auto PawnComponent = co_await WaitForComponent<UBattlePawnComponent>(BattlePawn);
		auto PawnLife = co_await PawnComponent->GetLife();

		co_await PawnLife->GetbAlive().If(false);

		FToastHandle H = ToastWidget->Toast(FText::FromString(TEXT("부활 대기 중...")));
		TAbortableCoroutineHandle SelectCharacter
			= WhileComponentIsValid<UCharacterSetterComponent>(&ThisClass::ListenToSelectCharacterActionTrigger);

		co_await Awaitables::Forever();
	}

	FWeakCoroutine ListenToEditConfigActionTrigger()
	{
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[E]를 눌러서 방 설정을 변경할 수 있습니다.")), 99);

		while (true)
		{
			do
			{
				co_await OnEditConfigActionTriggered;
			}
			while (bDialogOpen.Get());

			TAbortableCoroutineHandle H = EditConfig();
			co_await Awaitables::AnyOf(OnEditConfigActionTriggered, H.Get());
		}
	}

	FWeakCoroutine ListenToStartGameActionTrigger()
	{
		auto GameStarter = co_await GetOwningPlayerController()->FindComponentByClass<UGameStarterComponent>();
		FToastHandle ToastHandle = ToastWidget->Toast(FText::FromString(TEXT("[S]를 눌러서 게임을 시작할 수 있습니다.")), 98);

		while (true)
		{
			do
			{
				co_await OnStartGameActionTriggered;
			}
			while (bDialogOpen.Get());

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

		while (true)
		{
			do
			{
				co_await OnSelectCharacterActionTriggered;
			}
			while (bDialogOpen.Get());

			TAbortableCoroutineHandle H = SelectCharacter();
			co_await Awaitables::AnyOf(OnSelectCharacterActionTriggered, H.Get());
		}
	}

	FWeakCoroutine EditConfig()
	{
		auto ConfigComponent = co_await GetOwningPlayerController()->FindComponentByClass<UBattleConfigComponent>();
		auto ConfigWidget = CreateWidget<UBattleConfigWidget>(GetOwningPlayerController(), ConfigWidgetClass);
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

		auto SelectCharacterWidget = CreateWidget<USelectCharacterWidget>(GetOwningPlayerController(), SelectCharacterWidgetClass);
		auto S = ScopedAddToViewport(SelectCharacterWidget);
		bDialogOpen = true;
		auto F = FinallyIfValid(this, [this]() { bDialogOpen = false; });

		const int32 Selection = co_await Awaitables::AnyOf(SelectCharacterWidget->OnManny, SelectCharacterWidget->OnQuinn);
		Selection == 0 ? CharacterSetter->ServerEquipManny() : CharacterSetter->ServerEquipQuinn();
		ReadySetter->ServerSetReady(true);
	}

	FWeakCoroutine ShowTeamScores()
	{
		auto BattleGameStateComponent = co_await GameStateComponent->GetBattleGameStateComponent();
		auto TeamScoresWidget = CreateWidget<UTeamScoresWidget>(GetOwningPlayerController(), TeamScoresWidgetClass);
		auto S = ScopedAddToViewport(TeamScoresWidget);

		TAbortPtr<UAreaSpawnerComponent> AreaSpawner = co_await BattleGameStateComponent->GetAreaSpawner();
		auto AreaStream = AreaSpawner->GetSpawnedAreas().CreateAddStream() | Awaitables::IfValid();

		while (true)
		{
			AAreaActor* Area = (co_await AreaStream).Unsafe();

			RunWeakCoroutine([Area, TeamScoresWidget = TeamScoresWidget]() -> FWeakCoroutine
			{
				co_await AddToWeakList(Area);
				co_await AddToWeakList(TeamScoresWidget);

				auto AreaAreaStream = Area->GetServerCalculatedArea().MakeStream();

				while (true)
				{
					const float AreaArea = co_await AreaAreaStream;

					TeamScoresWidget->FindOrSetTeamScore(
						Area->TeamComponent->GetTeamIndex().Get(),
						Area->GetAreaBaseColor().Get(),
						AreaArea);
				}
			});
		}
	}

	FWeakCoroutine ShowTime()
	{
		auto BattleGameStateComponent = co_await GameStateComponent->GetBattleGameStateComponent();
		auto TimeWidget = CreateWidget<UTimeWidget>(GetOwningPlayerController(), TimeWidgetClass);
		auto S = ScopedAddToViewport(TimeWidget);

		const float EndTime = BattleGameStateComponent->GameEndWorldTime;
		const float CurrentTime = GetWorld()->GetGameState<AGameStateBase2>()->GetLatestServerWorldTimeSeconds();

		for (int32 RemainingSeconds = (EndTime - CurrentTime + 1.f); RemainingSeconds >= 0; RemainingSeconds--)
		{
			co_await WorldTimerComponent->At(EndTime - RemainingSeconds);
			TimeWidget->SetSeconds(RemainingSeconds);
		}

		co_await Awaitables::Forever();
	}

	FWeakCoroutine ShowResults()
	{
		auto ResultWidget = CreateWidget<UBattleResultWidget>(GetOwningPlayerController(), ResultWidgetClass);
		auto S = ScopedAddToViewport(ResultWidget);

		co_await WaitForSeconds(GetWorld(), 3.f);
		ResultWidget->ShowScores();

		// 렉이 심하게 걸렸다가 회복된 경우 저번 매치는 이미 끝나서 GameStateComponent가 파괴되어있을 수 있음
		TAbortableCoroutineHandle<FWeakCoroutine> Handle;
		if (IsValid(GameStateComponent))
		{
			Handle = RunWeakCoroutine([&]() -> FWeakCoroutine
			{
				auto BattleGameStateComponent = co_await GameStateComponent->GetBattleGameStateComponent();
				auto Result = co_await BattleGameStateComponent->GetBattleResult();
				for (const FBattleResultItem& Each : Result.Items)
				{
					ResultWidget->TeamScoresWidget->FindOrSetTeamScore(Each.TeamIndex, Each.Color, Each.Area);
				}
				ResultWidget->OnScoresSet();
			});
		}
		else
		{
			// TODO 렉이 너무 걸렸어서 결과를 가져올 수 없습니다 표시
		}

		co_await ResultWidget->OnConfirmed;
	}

	template <typename ComponentType>
	FWeakCoroutine WhileComponentIsValid(auto FuncPtr)
	{
		return RunWeakCoroutine(this,
			MakeComponentStream<ComponentType>(GetOwningPlayerController())
			| Awaitables::IsValid()
			| Awaitables::WhileTrue([this, FuncPtr]() { return std::invoke(FuncPtr, this); }));
	}
};
