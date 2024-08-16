// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/WorldTimerComponent.h"
#include "BattleGameStateComponent.generated.h"


USTRUCT()
struct FBattleResultItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TeamIndex = -1;

	UPROPERTY()
	FLinearColor Color = FLinearColor::Black;

	UPROPERTY()
	float Area = 0.f;
};


USTRUCT()
struct FBattleResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBattleResultItem> Items;
};


UCLASS(Within=GameStateBase)
class UBattleGameStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UWorldTimerComponent* ServerWorldTimer;

	UPROPERTY()
	UPawnSpawnerComponent* ServerPawnSpawner;

	DECLARE_LIVE_DATA_GETTER_SETTER(AreaSpawner);

	UPROPERTY(Replicated)
	float GameEndWorldTime;

	TLiveDataView<TOptional<FBattleResult>> GetBattleResult() const
	{
		return BattleResult;
	}

	void SetBattleResult(const FBattleResult& Result)
	{
		check(GetOwner()->HasAuthority());
		
		RepBattleResult = Result;
		
		if (GetNetMode() != NM_DedicatedServer)
		{
			OnRep_BattleResult();
		}
	}

	void KillPawnsOfTeam(int32 TeamIndex)
	{
		for (ULifeComponent* Each : GetPawnLivesOfTeam(TeamIndex))
		{
			Each->SetbAlive(false);
		}
	}

	bool KillAreaIfNobodyAlive(int32 TeamIndex)
	{
		for (ULifeComponent* Each : GetPawnLivesOfTeam(TeamIndex))
		{
			if (Each->GetbAlive().Get())
			{
				// 살아있는 팀원이 한 명이라도 있으면 영역 파괴하지 않음
				return false;
			}
		}

		if (AAreaActor* ThisManArea = FindLivingAreaOfTeam(TeamIndex))
		{
			ThisManArea->LifeComponent->SetbAlive(false);
			return true;
		}

		return false;
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
		// 현재 클라이언트에 PawnSpawner를 Replicate하지 않음 필요해지면 추가
		check(GetNetMode() != NM_Client);

		TArray<ULifeComponent*> Ret;
		for (APawn* Each : ServerPawnSpawner->GetSpawnedPawns().Get())
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

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaSpawner)
	UAreaSpawnerComponent* RepAreaSpawner;
	mutable TLiveData<UAreaSpawnerComponent*&> AreaSpawner{RepAreaSpawner};

	UFUNCTION()
	void OnRep_AreaSpawner() { AreaSpawner.Notify(); }

	// TODO 이거 FOptionalBattleResult 만들면 단순화 가능
	// FOptionalBattleResult는 TOptionalFromThis<FBattleResult> 상속
	UPROPERTY(ReplicatedUsing=OnRep_BattleResult)
	FBattleResult RepBattleResult;
	mutable TLiveData<TOptional<FBattleResult>> BattleResult;

	UFUNCTION()
	void OnRep_BattleResult()
	{
		if (RepBattleResult.Items.Num() > 0)
		{
			BattleResult.SetValueNoComparison(RepBattleResult);
		}
		else
		{
			BattleResult.SetValueNoComparison(TOptional<FBattleResult>{});
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, GameEndWorldTime, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepAreaSpawner, COND_InitialOnly);
		DOREPLIFETIME(ThisClass, RepBattleResult);
	}

	virtual void AttachServerMachineComponents() override
	{
		ServerWorldTimer = NewChildComponent<UWorldTimerComponent>(GetOuterAGameStateBase());
		ServerWorldTimer->RegisterComponent();

		AreaSpawner = NewChildComponent<UAreaSpawnerComponent>(GetOuterAGameStateBase());
		AreaSpawner->RegisterComponent();

		ServerPawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOuterAGameStateBase());
		ServerPawnSpawner->RegisterComponent();
	}
};
