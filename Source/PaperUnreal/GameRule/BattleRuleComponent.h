// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/AreaStateTrackerComponent.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/ModeAgnostic/PlayerSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/ModeAgnostic/TeamComponent.h"
#include "PaperUnreal/ModeAgnostic/WorldTimerComponent.h"
#include "PaperUnreal/WeakCoroutine/AwaitableWrappers.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "PaperUnreal/WeakCoroutine/ValueStream.h"
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
class UBattleRuleComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetPawnClass(UClass* Class)
	{
		check(!HasBegunPlay());
		PawnClass = Class;
	}

	TCancellableFuture<FBattleRuleResult> Start(int32 TeamCount, int32 EachTeamMemberCount)
	{
		check(HasBegunPlay());

		WorldTimer = GetWorld()->GetGameState()->FindComponentByClass<UWorldTimerComponent>();
		AreaSpawner = GetWorld()->GetGameState()->FindComponentByClass<UAreaSpawnerComponent>();
		PlayerSpawner = GetWorld()->GetGameState()->FindComponentByClass<UPlayerSpawnerComponent>();

		// 이 컴포넌트들은 디펜던시: 이 컴포넌트들을 가지는 GameState에 대해서만 이 클래스를 사용할 수 있음
		check(AllValid(WorldTimer, AreaSpawner, PlayerSpawner));

		TeamAllocator.Configure(TeamCount, EachTeamMemberCount);

		auto PlayerStateArray = GetWorld()->GetGameState<AGameStateBase2>()->GetPlayerStateArray();

		PlayerStateArray.ObserveAdd(this, [this](APlayerState* Player)
		{
			InitiatePlayerSpawnSequence(Player);
		});

		PlayerStateArray.ObserveRemove(this, [this](APlayerState* Player)
		{
			const int32 TeamIndex = Player->FindComponentByClass<UTeamComponent>()->GetTeamIndex().Get();
			if (TeamIndex >= 0)
			{
				UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어가 접속을 종료함에 따라 영역 파괴를 검토하는 중"), Player);
				KillAreaIfNobodyAlive(TeamIndex);
			}
		});

		return InitiateGameFlow();
	}

private:
	UPROPERTY()
	UClass* PawnClass;

	UPROPERTY()
	UWorldTimerComponent* WorldTimer;

	UPROPERTY()
	UAreaSpawnerComponent* AreaSpawner;

	UPROPERTY()
	UPlayerSpawnerComponent* PlayerSpawner;

	FTeamAllocator TeamAllocator;

	const TSoftObjectPtr<UMaterialInstance> SoftSolidBlue{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue.MI_Solid_Blue'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidBlueLight{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue_Light.MI_Solid_Blue_Light'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidRed{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red.MI_Solid_Red'")}};
	const TSoftObjectPtr<UMaterialInstance> SoftSolidRedLight{FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red_Light.MI_Solid_Red_Light'")}};

	const TArray<TSoftObjectPtr<UMaterialInstance>> AreaMaterials
	{
		SoftSolidBlue,
		SoftSolidRed,
	};

	const TArray<TSoftObjectPtr<UMaterialInstance>> TracerMaterials
	{
		SoftSolidBlueLight,
		SoftSolidRedLight,
	};

	void InitiatePlayerSpawnSequence(APlayerState* ReadyPlayer)
	{
		RunWeakCoroutine(this, [this, ReadyPlayer](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(ReadyPlayer);

			auto TeamComponent = ReadyPlayer->FindComponentByClass<UTeamComponent>();
			auto ReadyState = ReadyPlayer->FindComponentByClass<UReadyStateComponent>();
			auto Inventory = ReadyPlayer->FindComponentByClass<UInventoryComponent>();

			// 이 컴포넌트들은 디펜던시: 이 컴포넌트들을 가지는 PlayerState에 대해서만 이 클래스를 사용할 수 있음
			check(AllValid(TeamComponent, ReadyState, Inventory));

			UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어의 준비 완료를 기다리는 중"), ReadyPlayer);
			co_await AbortOnError(ReadyState->GetbReady().If(true));

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

			Inventory->SetHomeArea(ThisPlayerArea);
			Inventory->SetTracerMaterial(TracerMaterials[ThisPlayerTeamIndex % TracerMaterials.Num()]);

			UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어 폰을 스폰합니다"), ReadyPlayer);
			auto AreaBoundary = ThisPlayerArea->FindComponentByClass<UAreaBoundaryComponent>();
			APawn* Pawn = PlayerSpawner->SpawnAtLocation(PawnClass, AreaBoundary->GetRandomPointInside());
			ReadyPlayer->GetOwningController()->Possess(Pawn);

			UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어의 사망을 기다리는 중"), ReadyPlayer);
			Context.AbortIfNotValid(Pawn);
			co_await AbortOnError(Pawn->FindComponentByClass<ULifeComponent>()->GetbAlive().If(false));

			UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어가 사망함에 따라 영역 파괴를 검토하는 중"), ReadyPlayer);
			KillAreaIfNobodyAlive(ThisPlayerTeamIndex);

			constexpr float RespawnCool = 5.f;
			co_await AbortOnError(WaitForSeconds(GetWorld(), RespawnCool));
			Pawn->Destroy();

			UE_LOG(LogBattleRule, Log, TEXT("%p 플레이어 리스폰 시퀀스를 시작합니다"), ReadyPlayer);
			InitiatePlayerSpawnSequence(ReadyPlayer);
		});
	}

	TCancellableFuture<FBattleRuleResult> InitiateGameFlow()
	{
		return RunWeakCoroutine(this, [this](TWeakCoroutineContext<FBattleRuleResult>&) -> TWeakCoroutine<FBattleRuleResult>
		{
			UE_LOG(LogBattleRule, Log, TEXT("게임을 시작합니다"));

			auto Timeout = AbortOnError(RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
			{
				co_await AbortOnError(WorldTimer->At(GetWorld()->GetTimeSeconds() + 60.f));
			}));

			auto LastManStanding = AbortOnError(RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
			{
				auto AreaStateTracker = NewObject<UAreaStateTrackerComponent>(GetOwner());
				AreaStateTracker->SetSpawner(AreaSpawner);
				AreaStateTracker->RegisterComponent();
				co_await AbortOnError(AreaStateTracker->ZeroOrOneAreaIsSurviving());
				AreaStateTracker->DestroyComponent();
			}));

			const TOptional<int32> CompletedAwaitableIndex = co_await AnyOf(MoveTemp(Timeout), MoveTemp(LastManStanding));
			if (!CompletedAwaitableIndex)
			{
				co_return EDefaultFutureError::PromiseNotFulfilled;
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
			(AreaSpawner->GetSpawnedAreas().Get(), [&](auto Life, auto Team, auto Boundary)
			{
				if (Life->GetbAlive().Get())
				{
					const float Area = Boundary->GetBoundary()->CalculateArea();
					GameResult.SortedByRank.Emplace(Team->GetTeamIndex().Get(), Area);
				}
			});

			Algo::SortBy(GameResult.SortedByRank, &FBattleRuleResult::FTeamAndArea::Area);
			std::reverse(GameResult.SortedByRank.begin(), GameResult.SortedByRank.end());

			DestroyComponent();

			co_return GameResult;
		}).ReturnValue();
	}

	AAreaActor* InitializeNewAreaActor(int32 TeamIndex)
	{
		AAreaActor* Area = AreaSpawner->SpawnAreaAtRandomEmptyLocation();
		if (!Area)
		{
			return nullptr;
		}

		Area->TeamComponent->SetTeamIndex(TeamIndex);
		Area->SetAreaMaterial(AreaMaterials[TeamIndex % AreaMaterials.Num()]);

		RunWeakCoroutine(this, [this, Area, TeamIndex](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(Area);

			co_await AbortOnError(Area->LifeComponent->GetbAlive().If(false));
			
			UE_LOG(LogBattleRule, Log, TEXT("영역이 파괴됨에 따라 팀 %d의 모든 유저를 죽입니다."), TeamIndex);
			for (ULifeComponent* Each : GetPawnLivesOfTeam(TeamIndex))
			{
				Each->SetbAlive(false);
			}

			// 영역이 데스 애니메이션 등을 플레이하는데 충분한 시간을 준다
			co_await AbortOnError(WaitForSeconds(GetWorld(), 10.f));
			Area->Destroy();
		});

		return Area;
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

	AAreaActor* FindLivingAreaOfTeam(int32 TeamIndex) const
	{
		return ValidOrNull(AreaSpawner->GetSpawnedAreas().Get().FindByPredicate([&](AAreaActor* Each)
		{
			return IsValid(Each)
				&& Each->LifeComponent->GetbAlive().Get()
				&& Each->TeamComponent->GetTeamIndex().Get() == TeamIndex;
		}));
	}

	TArray<ULifeComponent*> GetPawnLivesOfTeam(int32 TeamIndex) const
	{
		TArray<ULifeComponent*> Ret;
		for (APawn* Each : PlayerSpawner->GetSpawnedPlayers().Get())
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
