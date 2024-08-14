// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattleGameStateComponent.h"
#include "Algo/Accumulate.h"
#include "PaperUnreal/GameFramework2/GameModeComponent.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleConfigComponent.h"
#include "PaperUnreal/GameMode/BattleGameMode/BattleGameModeComponent.h"
#include "PaperUnreal/GameMode/FreeGameMode/FreeGameModeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterSetterComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/GameStarterComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/PrivilegeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ReadyStateComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "PVPBattleGameModeComponent.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPVPBattleGameMode, Log, All);


namespace PVPBattlePrivilege
{
	inline FName Host{TEXT("PVPBattlePrivilege_Host")};
	inline FName Normie{TEXT("PVPBattlePrivilege_Normie")};
}


namespace PVPBattleStage
{
	inline FName WaitingForStart{TEXT("PVPBattleStage_WaitingForStart")};
	inline FName WillPlay{TEXT("PVPBattleStage_WillPlay")};
	inline FName Playing{TEXT("PVPBattleStage_Playing")};
	inline FName Result{TEXT("PVPBattleStage_Result")};
}


UCLASS()
class UPVPBattleGameModeComponent : public UGameModeComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UPVPBattleGameStateComponent* GameStateComponent;
	
	UPROPERTY()
	UWorldTimerComponent* WorldTimerComponent;

	UPROPERTY()
	TArray<UPrivilegeComponent*> _0;
	TLiveData<TArray<UPrivilegeComponent*>&> Privileges{_0};

	UPROPERTY()
	TArray<UReadyStateComponent*> ReadyStates;

	FSimpleMulticastDelegate OnGameStartConditionsMet;

	virtual void AttachServerMachineComponents() override
	{
		GameStateComponent = NewChildComponent<UPVPBattleGameStateComponent>(GetGameState());
		WorldTimerComponent = NewChildComponent<UWorldTimerComponent>(GetGameState());
	}

	virtual void AttachPlayerControllerComponents(APlayerController* PC) override
	{
		auto Privilege = NewChildComponent<UPrivilegeComponent>(PC);
		Privilege->AddComponentForPrivilege(PVPBattlePrivilege::Host, UBattleConfigComponent::StaticClass());
		Privilege->AddComponentForPrivilege(PVPBattlePrivilege::Host, UGameStarterComponent::StaticClass(),
			FComponentInitializer::CreateWeakLambda(this, [this](UActorComponent* Component)
			{
				Cast<UGameStarterComponent>(Component)
					->ServerGameStarter.BindUObject(this, &ThisClass::StartGameIfConditionsMet);
			}));
		Privilege->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UCharacterSetterComponent::StaticClass());
		Privilege->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UReadySetterComponent::StaticClass());
		Privilege->RegisterComponent();
		Privileges.Add(Privilege);
	}

	virtual void AttachPlayerStateComponents(APlayerState* PS) override
	{
		auto ReadyState = NewChildComponent<UReadyStateComponent>(PS);
		ReadyState->RegisterComponent();
		ReadyStates.Add(ReadyState);
	}

	bool StartGameIfConditionsMet()
	{
		ReadyStates.RemoveAll([](UReadyStateComponent* Each) { return !IsValid(Each); });
		
		const int32 ReadyCount = Algo::TransformAccumulate(
			ReadyStates, [](auto Each) { return Each->GetbReady().Get() ? 1 : 0; }, 0);
		
		if (ReadyCount >= 2)
		{
			OnGameStartConditionsMet.Broadcast();
			return true;
		}

		return false;
	}

	FWeakCoroutine InitPrivilegeComponentOfNewPlayers()
	{
		auto PrivilegeStream = Privileges.CreateAddStream();

		{
			auto FirstPlayerPrivilege = co_await PrivilegeStream;
			FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Host);
			FirstPlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
		}

		while (true)
		{
			auto PlayerPrivilege = co_await PrivilegeStream;
			PlayerPrivilege->GivePrivilege(PVPBattlePrivilege::Normie);
		}
	}

	FWeakCoroutine InitiateGameFlow()
	{
		auto StageComponent = co_await GameStateComponent->StageComponent;

		StageComponent->SetCurrentStage(PVPBattleStage::WaitingForStart);

		{
			auto FreeGameMode = NewChildComponent<UFreeGameModeComponent>();
			FreeGameMode->RegisterComponent();
			FreeGameMode->Start();
			auto F = FinallyIfValid(FreeGameMode, [FreeGameMode]() { FreeGameMode->DestroyComponent(); });

			co_await OnGameStartConditionsMet;

			StageComponent->SetCurrentStage(PVPBattleStage::WillPlay);

			const float PlayStartTime = GetWorld()->GetTimeSeconds() + 5.f;
			StageComponent->SetStageWorldStartTime(PVPBattleStage::Playing, PlayStartTime);

			co_await WorldTimerComponent->At(PlayStartTime);
			StageComponent->SetCurrentStage(PVPBattleStage::Playing);
		}

		auto ResultComponent = co_await [&]()
		{
			auto BattleGameMode = NewChildComponent<UBattleGameModeComponent>();
			BattleGameMode->RegisterComponent();
			return BattleGameMode->Start(2, 2);
		}();

		StageComponent->SetCurrentStage(PVPBattleStage::Result);
		StageComponent->SetStageWorldStartTime(PVPBattleStage::Result, GetWorld()->GetTimeSeconds());

		for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
		{
			Each->GetPlayerController()->ChangeState(NAME_Spectating);
			Each->GetPlayerController()->ClientGotoState(NAME_Spectating);
		}

		// TODO 클라이언트들이 result를 보기에 충분한 시간을 준다
		// ResultComponent->DestroyComponent();
	}
};
