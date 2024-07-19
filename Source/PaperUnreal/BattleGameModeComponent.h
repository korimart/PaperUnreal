// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaSpawnerComponent.h"
#include "AreaStateTrackerComponent.h"
#include "InventoryComponent.h"
#include "PlayerSpawnerComponent.h"
#include "ReadyStateComponent.h"
#include "WorldTimerComponent.h"
#include "Core/ActorComponent2.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "BattleGameModeComponent.generated.h"


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


struct FBattleModeGameResult
{
};


UCLASS(Within=GameModeBase)
class UBattleGameModeComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetPawnClass(UClass* Class)
	{
		check(!HasBegunPlay());
		PawnClass = Class;
	}
	
	TCancellableFuture<FBattleModeGameResult> Start(int32 TeamCount, int32 EachTeamMemberCount)
	{
		check(HasBegunPlay());

		WorldTimer = GetWorld()->GetGameState()->FindComponentByClass<UWorldTimerComponent>();
		AreaSpawner = GetWorld()->GetGameState()->FindComponentByClass<UAreaSpawnerComponent>();
		PlayerSpawner = GetWorld()->GetGameState()->FindComponentByClass<UPlayerSpawnerComponent>();

		// 이 컴포넌트들은 디펜던시: 이 컴포넌트들을 가지는 GameState에 대해서만 이 클래스를 사용할 수 있음
		check(AllValid(WorldTimer, AreaSpawner, PlayerSpawner));

		TeamAllocator.Configure(TeamCount, EachTeamMemberCount);

		for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
		{
			InitiatePlayerSpawnSequence(Each);
		}

		FGameModeEvents::OnGameModePostLoginEvent().AddWeakLambda(this, [this](auto, APlayerController* PC)
		{
			InitiatePlayerSpawnSequence(PC->PlayerState);
		});

		InitiateGameFlow();

		auto [Promise, Future] = MakePromise<FBattleModeGameResult>();
		ResultPromise.Emplace(MoveTemp(Promise));
		return MoveTemp(Future);
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
	FBattleModeGameResult GameResult{};
	TOptional<TCancellablePromise<FBattleModeGameResult>> ResultPromise;
	
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

			co_await ReadyState->GetbReady().WaitForValue(true);

			// 죽은 다음에 다시 스폰하는 경우에는 팀이 이미 있음
			if (!TeamComponent->GetTeamIndex().GetValue().IsSet())
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
			
			const int32 ThisPlayerTeamIndex = *TeamComponent->GetTeamIndex();

			AAreaActor* ThisPlayerArea =
				ValidOrNull(AreaSpawner->GetSpawnedAreaStreamer().GetHistory().FindByPredicate([&](AAreaActor* Each)
				{
					return *Each->TeamComponent->GetTeamIndex().GetValue() == ThisPlayerTeamIndex;
				}));

			if (!ThisPlayerArea)
			{
				ThisPlayerArea = AreaSpawner->SpawnAreaAtRandomEmptyLocation();
				ThisPlayerArea->TeamComponent->SetTeamIndex(ThisPlayerTeamIndex);
				ThisPlayerArea->SetAreaMaterial(AreaMaterials[ThisPlayerTeamIndex % AreaMaterials.Num()]);
			}

			// 월드가 꽉차서 새 영역을 선포할 수 없음
			if (!ThisPlayerArea)
			{
				// TODO 일단 이 팀은 플레이를 할 수 없는데 나중에 공간이 생길 수도 있음 그 때 스폰해주자
				co_return;
			}

			Inventory->SetHomeArea(ThisPlayerArea);
			Inventory->SetTracerMaterial(TracerMaterials[ThisPlayerTeamIndex % TracerMaterials.Num()]);

			AActor* Pawn = PlayerSpawner->SpawnAtLocation(PawnClass, ThisPlayerArea->GetActorTransform().GetLocation());
			ReadyPlayer->GetOwningController()->Possess(CastChecked<APawn>(Pawn));
		});
	}

	void InitiateGameFlow()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto Timeout = AbortOnError(RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
			{
				co_await WorldTimer->At(GetWorld()->GetTimeSeconds() + 60.f);
			}));
			
			auto OnlyOneTeamSurviving = AbortOnError(RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
			{
				auto AreaStateTracker = NewObject<UAreaStateTrackerComponent>(GetOwner());
				AreaStateTracker->SetSpawner(AreaSpawner);
				AreaStateTracker->RegisterComponent();
				
				auto F = FinallyIfValid(AreaStateTracker, [AreaStateTracker](){ AreaStateTracker->DestroyComponent(); });

				co_await AreaStateTracker->OnlyOneAreaIsSurviving();
			}));
		
			const TOptional<int32> CompletedAwaitableIndex = co_await AnyOf(MoveTemp(Timeout), MoveTemp(OnlyOneTeamSurviving));
			if (!CompletedAwaitableIndex)
			{
				co_return;
			}

			// TODO end game
		});
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);

		if (ResultPromise)
		{
			ResultPromise->SetValue(MoveTemp(GameResult));
		}
	}
};
