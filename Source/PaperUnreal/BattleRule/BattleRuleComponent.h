// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleRuleGameStateComponent.h"
#include "BattleRulePawnComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/AreaStateTrackerComponent.h"
#include "PaperUnreal/Development/InGameCheats.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/TeamComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
#include "PaperUnreal/WeakCoroutine/AnyOfAwaitable.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "BattleRuleComponent.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogBattleRule, Log, All);


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


struct FBattleRuleResult
{
	struct FTeamAndArea
	{
		int32 TeamIndex;
		float Area;
	};

	TArray<FTeamAndArea> SortedByRank;
};


UCLASS(Within=GameModeBase)
class UBattleRuleComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	TWeakCoroutine<FBattleRuleResult> Start(UClass* InPawnClass, int32 TeamCount, int32 EachTeamMemberCount)
	{
		check(HasBegunPlay());

		PawnClass = InPawnClass;
		TeamAllocator.Configure(TeamCount, EachTeamMemberCount);

		GameState = NewChildComponent<UBattleRuleGameStateComponent>(GetWorld()->GetGameState());
		GameState->RegisterComponent();

		auto PlayerStateArray = GetWorld()->GetGameState<AGameStateBase2>()->GetPlayerStateArray();

		PlayerStateArray.ObserveAddIfValid(this, [this](APlayerState* Player)
		{
			InitiatePawnSpawnSequence(Player);
		});

		PlayerStateArray.ObserveRemoveIfValid(this, [this](APlayerState* Player)
		{
			if (APawn* Pawn = Player->GetPawn())
			{
				if (auto PawnComponent = Pawn->FindComponentByClass<UBattleRulePawnComponent>())
				{
					UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어가 접속을 종료함에 따라 폰을 사망시킵니다"), Player);
					PawnComponent->GetServerLife()->SetbAlive(false);
				}
			}
		});

		return InitiateGameFlow();
	}

private:
	UPROPERTY()
	UClass* PawnClass;

	UPROPERTY()
	UBattleRuleGameStateComponent* GameState;

	FTeamAllocator TeamAllocator;
	TMap<int32, FLinearColor> TeamColors;

	FWeakCoroutine InitiatePawnSpawnSequence(APlayerState* Player)
	{
		co_await AddToWeakList(Player);

		auto TeamComponent = Player->FindComponentByClass<UTeamComponent>();
		auto ReadyState = Player->FindComponentByClass<UReadyStateComponent>();
		auto Inventory = Player->FindComponentByClass<UInventoryComponent>();

		// 이 컴포넌트들은 디펜던시: 이 컴포넌트들을 가지는 PlayerState에 대해서만 이 클래스를 사용할 수 있음
		check(AllValid(TeamComponent, ReadyState, Inventory));

		UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어의 준비 완료를 기다리는 중"), Player);
		co_await ReadyState->GetbReady().If(true);

		// 죽은 다음에 다시 스폰하는 경우에는 팀이 이미 있음
		if (TeamComponent->GetTeamIndex().Get() < 0)
		{
			if (TOptional<int32> NextTeamIndex = TeamAllocator.NextTeamIndex())
			{
				TeamComponent->SetTeamIndex(*NextTeamIndex);
			}
			else
			{
				// TODO 꽉차서 더 이상 입장할 수 없으니 플레이어한테 알려주던가 그런 처리
				co_return;
			}
		}

		const int32 ThisPlayerTeamIndex = TeamComponent->GetTeamIndex().Get();
		AAreaActor* ThisPlayerArea = FindLivingAreaOfTeam(ThisPlayerTeamIndex);

		if (!ThisPlayerArea)
		{
			ThisPlayerArea = InitializeNewAreaActor(ThisPlayerTeamIndex);
		}

		// 월드가 꽉차서 새 영역을 선포할 수 없음
		if (!ThisPlayerArea)
		{
			// TODO 일단 이 팀은 플레이를 할 수 없는데 나중에 공간이 생길 수도 있음 그 때 스폰해주자
			co_return;
		}

		Inventory->SetTracerBaseColor(ALittleBrighter(TeamColors[ThisPlayerTeamIndex]));

		UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어 폰을 스폰합니다"), Player);
		APawn* Pawn = GameState->ServerPawnSpawner->SpawnAtLocation(
			PawnClass,
			ThisPlayerArea->ServerAreaBoundary->GetRandomPointInside(),
			[&](APawn* ToInit)
			{
				auto PawnComponent = NewObject<UBattleRulePawnComponent>(ToInit);
				PawnComponent->SetDependencies(GameState, ThisPlayerArea);
				PawnComponent->RegisterComponent();
			});
		Player->GetOwningController()->Possess(Pawn);

		co_await InitiatePawnLifeSequence(Pawn->FindComponentByClass<UBattleRulePawnComponent>(), ThisPlayerTeamIndex);

		constexpr float RespawnCool = 5.f;
		co_await WaitForSeconds(GetWorld(), RespawnCool);

		UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어 리스폰 시퀀스를 시작합니다"), Player);
		InitiatePawnSpawnSequence(Player);
	}

	FWeakCoroutine InitiatePawnLifeSequence(UBattleRulePawnComponent* Pawn, int32 TeamIndex) const
	{
		co_await AddToWeakList(Pawn);

		UE_LOG(LogBattleRule, Log, TEXT("%p 폰의 사망을 기다리는 중"), Pawn);
		co_await Pawn->GetServerLife()->GetbAlive().If(false);

		UE_LOG(LogBattleRule, Log, TEXT("%p 폰이 사망함에 따라 영역 파괴를 검토하는 중"), Pawn);
		KillAreaIfNobodyAlive(TeamIndex);
	}

	TWeakCoroutine<FBattleRuleResult> InitiateGameFlow()
	{
		UE_LOG(LogBattleRule, Log, TEXT("게임을 시작합니다"));

		auto F = FinallyIfValid(this, [this]() { DestroyComponent(); });

		auto Timeout = RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await GameState->ServerWorldTimer->At(GetWorld()->GetTimeSeconds() + 60.f);
		});

		auto LastManStanding = RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto AreaStateTracker = NewObject<UAreaStateTrackerComponent>(GetOwner());
			AreaStateTracker->SetSpawner(GameState->GetAreaSpawner().Get());
			AreaStateTracker->RegisterComponent();
			co_await AreaStateTracker->ZeroOrOneAreaIsSurviving();
			AreaStateTracker->DestroyComponent();
		});

		const int32 CompletedAwaitableIndex = co_await Awaitables::AnyOf(
			Timeout, LastManStanding, UInGameCheats::OnEndGameByCheat);

		if (CompletedAwaitableIndex != 2 && UInGameCheats::bDoNotEndGameUntilCheat)
		{
			UE_LOG(LogBattleRule, Log, TEXT("게임 끝났지만 치트를 기다리는 중"));
			co_await UInGameCheats::OnEndGameByCheat;
		}

		if (CompletedAwaitableIndex == 0)
		{
			UE_LOG(LogBattleRule, Log, TEXT("제한시간이 끝나 게임을 종료합니다"));
		}
		else if (CompletedAwaitableIndex == 1)
		{
			UE_LOG(LogBattleRule, Log, TEXT("팀의 수가 1이하이므로 게임을 종료합니다"));
		}

		// 모종의 이유로 에리어 두 개가 동시에 파괴돼서 무승부가 될 수도 있음 무승부면 결과가 빔
		// TODO 마지막까지 남은 에리어들을 알아내서 이기게 해줘야 됨
		FBattleRuleResult GameResult{};

		ForEachComponent<ULifeComponent, UTeamComponent, UAreaBoundaryComponent>
		(GameState->GetAreaSpawner().Get()->GetSpawnedAreas().Get(), [&](auto Life, auto Team, auto Boundary)
		{
			if (Life->GetbAlive().Get())
			{
				const float Area = Boundary->GetBoundary().Get().CalculateArea();
				GameResult.SortedByRank.Emplace(Team->GetTeamIndex().Get(), Area);
			}
		});

		Algo::SortBy(GameResult.SortedByRank, &FBattleRuleResult::FTeamAndArea::Area);
		std::reverse(GameResult.SortedByRank.begin(), GameResult.SortedByRank.end());

		co_return GameResult;
	}

	AAreaActor* InitializeNewAreaActor(int32 TeamIndex)
	{
		return GameState->GetAreaSpawner().Get()->SpawnAreaAtRandomEmptyLocation([&](AAreaActor* Area)
		{
			Area->TeamComponent->SetTeamIndex(TeamIndex);

			if (!TeamColors.Contains(TeamIndex))
			{
				TeamColors.FindOrAdd(TeamIndex) = NonEyeSoaringRandomColor();
			}

			Area->SetAreaBaseColor(TeamColors.FindRef(TeamIndex));

			RunWeakCoroutine(this, [this, Area, TeamIndex]() -> FWeakCoroutine
			{
				co_await AddToWeakList(Area);

				co_await Area->LifeComponent->GetbAlive().If(false);

				UE_LOG(LogBattleRule, Log, TEXT("영역이 파괴됨에 따라 팀 %d의 모든 유저를 죽입니다."), TeamIndex);
				for (ULifeComponent* Each : GetPawnLivesOfTeam(TeamIndex))
				{
					Each->SetbAlive(false);
				}

				// 영역이 데스 애니메이션 등을 플레이하는데 충분한 시간을 준다
				co_await WaitForSeconds(GetWorld(), 10.f);
				Area->Destroy();
			});
		});
	}

	void KillAreaIfNobodyAlive(int32 TeamIndex) const
	{
		for (ULifeComponent* Each : GetPawnLivesOfTeam(TeamIndex))
		{
			if (Each->GetbAlive().Get())
			{
				// 살아있는 팀원이 한 명이라도 있으면 영역 파괴하지 않음
				return;
			}
		}

		if (AAreaActor* ThisManArea = FindLivingAreaOfTeam(TeamIndex))
		{
			UE_LOG(LogBattleRule, Log, TEXT("팀 %d에 살아있는 유저가 없어 파괴합니다"), TeamIndex);
			ThisManArea->LifeComponent->SetbAlive(false);
		}
	}

	// TODO move to game state
	AAreaActor* FindLivingAreaOfTeam(int32 TeamIndex) const
	{
		return ValidOrNull(GameState->GetAreaSpawner().Get()->GetSpawnedAreas().Get().FindByPredicate([&](AAreaActor* Each)
		{
			return IsValid(Each)
				&& Each->LifeComponent->GetbAlive().Get()
				&& Each->TeamComponent->GetTeamIndex().Get() == TeamIndex;
		}));
	}

	// TODO move to game state
	TArray<ULifeComponent*> GetPawnLivesOfTeam(int32 TeamIndex) const
	{
		TArray<ULifeComponent*> Ret;
		for (APawn* Each : GameState->ServerPawnSpawner->GetSpawnedPawns().Get())
		{
			if (!IsValid(Each) || !Each->GetPlayerState())
			{
				continue;
			}

			auto TeamComponent = Each->GetPlayerState()->FindComponentByClass<UTeamComponent>();

			if (TeamComponent->GetTeamIndex().Get() == TeamIndex)
			{
				Ret.Add(Each->FindComponentByClass<ULifeComponent>());
			}
		}
		return Ret;
	}
};
