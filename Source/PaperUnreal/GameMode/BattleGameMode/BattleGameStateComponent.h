// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/WorldTimerComponent.h"
#include "BattleGameStateComponent.generated.h"


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
		return ValidOrNull(AreaSpawner.Get()->GetSpawnedAreas().Get().FindByPredicate([&](AAreaActor* Each)
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
	UAreaSpawnerComponent* _;
	mutable TLiveData<UAreaSpawnerComponent*&> AreaSpawner{_};

	UFUNCTION()
	void OnRep_AreaSpawner() { AreaSpawner.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, _, COND_InitialOnly);
	}

	virtual void AttachServerMachineComponents() override
	{
		ServerWorldTimer = NewChildComponent<UWorldTimerComponent>(GetOuterAGameStateBase());
		ServerWorldTimer->RegisterComponent();

		AreaSpawner = NewChildComponent<UAreaSpawnerComponent>(GetOuterAGameStateBase());
		AreaSpawner.Get()->RegisterComponent();

		ServerPawnSpawner = NewChildComponent<UPawnSpawnerComponent>(GetOuterAGameStateBase());
		ServerPawnSpawner->RegisterComponent();
	}
};
