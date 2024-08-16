// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleGameStateComponent.h"
#include "BattlePawnComponent.h"
#include "BattlePlayerStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/AreaStateTrackerComponent.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/GameModeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/TeamComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/WorldTimerComponent.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "BattleGameModeComponent.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogBattleGameMode, Log, All);


class FTeamAllocator
{
public:
	void Configure(int32 TeamCount, int32 EachTeamMemberCount)
	{
		MaxTeamCount = TeamCount;
		MaxMemberCount = EachTeamMemberCount;
	}

	TOptional<int32> NextTeamIndex()
	{
		// TODO 중간에 나가는 인원이 있으면 그 빈자리를 주도록 구현 변경
		const int32 Index = NextIndex++;
		const int32 TeamIndex = Index % MaxTeamCount;
		return Index < MaxTeamCount * MaxMemberCount ? TeamIndex : TOptional<int32>{};
	}

private:
	int32 NextIndex = 0;
	int32 MaxTeamCount = 0;
	int32 MaxMemberCount = 0;
};


UCLASS(Within=GameModeBase)
class UBattleGameModeComponentImpl : public UGameModeComponent
{
	GENERATED_BODY()

public:
	void Configure(int32 TeamCount, int32 EachTeamMemberCount)
	{
		check(!bConfigured.Get());
		TeamAllocator.Configure(TeamCount, EachTeamMemberCount);
		bConfigured = true;
	}

	FWeakCoroutine Start(UBattleGameStateComponent* InGameStateComponent)
	{
		check(HasBegunPlay());
		GameStateComponent = InGameStateComponent;
		return InitiateGameFlow();
	}

private:
	UPROPERTY()
	UBattleGameStateComponent* GameStateComponent;

	FTeamAllocator TeamAllocator;
	TMap<int32, FLinearColor> TeamColors;

	TLiveData<bool> bConfigured{false};
	TLiveData<bool> bGameStarted{false};

	virtual void OnPostLogin(APlayerController* PC) override
	{
		// 디펜던시: parent game mode에서 미리 만들었을 것이라고 가정함
		check(!!PC->PlayerState->FindComponentByClass<UReadyStateComponent>());

		NewChildComponent<UBattlePlayerStateComponent>(PC->PlayerState)->RegisterComponent();
		InitiatePlayerSequence(PC);
	}

	virtual void OnPostLogout(APlayerController* PC) override
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (auto PawnComponent = Pawn->FindComponentByClass<UBattlePawnComponent>())
			{
				UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어가 접속을 종료함에 따라 폰을 사망시킵니다"), PC);
				PawnComponent->GetLife().Get()->SetbAlive(false);
			}
		}
	}

	FWeakCoroutine InitiatePlayerSequence(APlayerController* Player)
	{
		co_await AddToWeakList(Player);

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어 입장 게임설정을 기다리는 중"), Player);
		co_await (bConfigured.MakeStream() | Awaitables::If(true));

		auto ReadyState = Player->PlayerState->FindComponentByClass<UReadyStateComponent>();
		auto PlayerStateComponent = Player->PlayerState->FindComponentByClass<UBattlePlayerStateComponent>();
		auto TeamComponent = PlayerStateComponent->ServerTeamComponent;
		auto Inventory = PlayerStateComponent->GetInventoryComponent().Get();

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어의 준비 완료를 기다리는 중"), Player);
		co_await ReadyState->GetbReady().If(true);

		if (TOptional<int32> NextTeamIndex = TeamAllocator.NextTeamIndex())
		{
			TeamComponent->SetTeamIndex(*NextTeamIndex);
		}
		else
		{
			// TODO 꽉차서 더 이상 입장할 수 없으니 플레이어한테 알려주던가 그런 처리
			co_return;
		}

		const int32 ThisPlayerTeamIndex = TeamComponent->GetTeamIndex().Get();

		if (!TeamColors.Contains(ThisPlayerTeamIndex))
		{
			TeamColors.FindOrAdd(ThisPlayerTeamIndex) = NonEyeSoaringRandomColor();
		}

		Inventory->SetTracerBaseColor(ALittleBrighter(TeamColors[ThisPlayerTeamIndex]));

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어 팀 세팅 완료 게임 시작을 기다리는 중"), Player);
		co_await (bGameStarted.MakeStream() | Awaitables::If(true));

		InitiatePawnSpawnSequence(Player, ThisPlayerTeamIndex);
	}

	FWeakCoroutine InitiatePawnSpawnSequence(APlayerController* Player, int32 TeamIndex)
	{
		co_await AddToWeakList(Player);

		AAreaActor* ThisPlayerArea = GameStateComponent->FindLivingAreaOfTeam(TeamIndex);
		if (!ThisPlayerArea)
		{
			ThisPlayerArea = InitializeNewAreaActor(TeamIndex);
		}

		// 월드가 꽉차서 새 영역을 선포할 수 없음
		if (!ThisPlayerArea)
		{
			// TODO 일단 이 팀은 플레이를 할 수 없는데 나중에 공간이 생길 수도 있음 그 때 스폰해주자
			co_return;
		}

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어 폰을 스폰합니다"), Player);
		APawn* Pawn = GameStateComponent->ServerPawnSpawner->SpawnAtLocation(
			GetDefaultPawnClass(),
			ThisPlayerArea->ServerAreaBoundary->GetRandomPointInside(),
			[&](APawn* ToInit)
			{
				auto PawnComponent = NewObject<UBattlePawnComponent>(ToInit);
				PawnComponent->SetDependencies(GameStateComponent, ThisPlayerArea);
				PawnComponent->RegisterComponent();
			});
		Player->Possess(Pawn);

		co_await InitiatePawnLifeSequence(Pawn->FindComponentByClass<UBattlePawnComponent>(), TeamIndex);

		constexpr float RespawnCool = 5.f;
		co_await WaitForSeconds(GetWorld(), RespawnCool);

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 플레이어 리스폰 시퀀스를 시작합니다"), Player);
		InitiatePawnSpawnSequence(Player, TeamIndex);
	}

	FWeakCoroutine InitiatePawnLifeSequence(UBattlePawnComponent* Pawn, int32 TeamIndex) const
	{
		co_await AddToWeakList(Pawn);

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 폰의 사망을 기다리는 중"), Pawn);
		co_await Pawn->GetLife().Get()->GetbAlive().If(false);

		UE_LOG(LogBattleGameMode, Log, TEXT("%p 폰이 사망함에 따라 영역 파괴를 검토하는 중"), Pawn);
		if (GameStateComponent->KillAreaIfNobodyAlive(TeamIndex))
		{
			UE_LOG(LogBattleGameMode, Log, TEXT("팀 %d에 살아있는 유저가 없어 파괴합니다"), TeamIndex);
		}
	}

	FWeakCoroutine InitiateGameFlow()
	{
		UE_LOG(LogBattleGameMode, Log, TEXT("게임을 시작합니다"));
		bGameStarted = true;

		auto F = FinallyIfValid(this, [this]() { DestroyComponent(); });

		auto Timeout = RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			const float GameEndWorldTime = GetWorld()->GetTimeSeconds() + 60.f;
			GameStateComponent->GameEndWorldTime = GameEndWorldTime;
			co_await GameStateComponent->ServerWorldTimer->At(GameEndWorldTime);
		});

		auto LastManStanding = RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto AreaStateTracker = NewObject<UAreaStateTrackerComponent>(GetOwner());
			AreaStateTracker->SetSpawner(GameStateComponent->GetAreaSpawner().Get());
			AreaStateTracker->RegisterComponent();
			co_await AreaStateTracker->ZeroOrOneAreaIsSurviving();
			AreaStateTracker->DestroyComponent();
		});

		const int32 CompletedAwaitableIndex = co_await Awaitables::AnyOf(
			Timeout, LastManStanding, UInGameCheats::OnEndGameByCheat);

		if (CompletedAwaitableIndex != 2 && UInGameCheats::bDoNotEndGameUntilCheat)
		{
			UE_LOG(LogBattleGameMode, Log, TEXT("게임 끝났지만 치트를 기다리는 중"));
			co_await UInGameCheats::OnEndGameByCheat;
		}

		if (CompletedAwaitableIndex == 0)
		{
			UE_LOG(LogBattleGameMode, Log, TEXT("제한시간이 끝나 게임을 종료합니다"));
		}
		else if (CompletedAwaitableIndex == 1)
		{
			UE_LOG(LogBattleGameMode, Log, TEXT("팀의 수가 1이하이므로 게임을 종료합니다"));
		}

		FBattleResult Result;
		for (const auto& [TeamIndex, Color] : TeamColors)
		{
			AAreaActor* Area = GameStateComponent->FindLivingAreaOfTeam(TeamIndex);
			const float AreaArea = Area ? Area->GetServerCalculatedArea().Get() : 0.f;
			Result.Items.Add({
				.TeamIndex = TeamIndex,
				.Color = Color,
				.Area = AreaArea,
			});
		}

		// TODO 두 에리어가 동시에 파괴되면 무승부가 발생할 수 있음
		Algo::SortBy(Result.Items, &FBattleResultItem::Area, TGreater{});
		
		GameStateComponent->SetBattleResult(Result);
	}

	AAreaActor* InitializeNewAreaActor(int32 TeamIndex)
	{
		return GameStateComponent->GetAreaSpawner().Get()->SpawnAreaAtRandomEmptyLocation([&](AAreaActor* Area)
		{
			Area->TeamComponent->SetTeamIndex(TeamIndex);
			Area->SetAreaBaseColor(TeamColors.FindRef(TeamIndex));

			RunWeakCoroutine(this, [this, Area, TeamIndex]() -> FWeakCoroutine
			{
				co_await AddToWeakList(Area);

				co_await Area->LifeComponent->GetbAlive().If(false);

				UE_LOG(LogBattleGameMode, Log, TEXT("영역이 파괴됨에 따라 팀 %d의 모든 유저를 죽입니다."), TeamIndex);
				GameStateComponent->KillPawnsOfTeam(TeamIndex);

				// 영역이 데스 애니메이션 등을 플레이하는데 충분한 시간을 준다
				co_await WaitForSeconds(GetWorld(), 10.f);
				Area->Destroy();
			});
		});
	}
};


UCLASS(Within=GameModeBase)
class UBattleGameModeComponent : public UGameModeComponent
{
	GENERATED_BODY()

public:
	void Configure(int32 TeamCount, int32 EachTeamMemberCount)
	{
		check(!IsValid(Impl));
		Impl = NewChildComponent<UBattleGameModeComponentImpl>();
		Impl->RegisterComponent();
		Impl->Configure(TeamCount, EachTeamMemberCount);
	}

	TCancellableFuture<void> Start()
	{
		GameStateComponent = NewChildComponent<UBattleGameStateComponent>(GetWorld()->GetGameState());
		GameStateComponent->RegisterComponent();

		// Caller가 Impl이 반환하는 FWeakCoroutine으로 게임을 주물럭거리지 않게 하기 위해 Future로 변환
		auto [Promise, Future] = MakePromise<void>();
		Impl->Start(GameStateComponent).Then([Promise = MoveTemp(Promise)](const auto&) mutable { Promise.SetValue(); });
		return MoveTemp(Future);
	}
	
	UBattleGameStateComponent* GetGameStateComponent() const
	{
		return GameStateComponent;
	}

private:
	UPROPERTY()
	UBattleGameModeComponentImpl* Impl;
	
	UPROPERTY()
	UBattleGameStateComponent* GameStateComponent;
};