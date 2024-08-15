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
	TArray<UPrivilegeComponent*> Privileges_Backing;
	TLiveData<TArray<UPrivilegeComponent*>&> Privileges{Privileges_Backing};

	UPROPERTY()
	TArray<UReadyStateComponent*> ReadyStates;

	friend class UInGameCheats;
	FSimpleMulticastDelegate OnGameStartConditionsMet;

	virtual void AttachServerMachineComponents() override
	{
		WorldTimerComponent = NewChildComponent<UWorldTimerComponent>(GetGameState());
		WorldTimerComponent->RegisterComponent();
	}

	virtual void OnPostLogin(APlayerController* PC) override
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

		auto ReadyState = NewChildComponent<UReadyStateComponent>(PC->PlayerState);
		ReadyState->RegisterComponent();
		ReadyStates.Add(ReadyState);
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		InitiatePrivilegeGranter();
		InitiateGameFlow();
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

	FWeakCoroutine InitiatePrivilegeGranter()
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
		UBattleGameModeComponent* PrevBattleGameModeComponent = nullptr;
		UPVPBattleGameStateComponent* PrevGameStateComponent = nullptr;

		while (true)
		{
			GameStateComponent = NewChildComponent<UPVPBattleGameStateComponent>(GetGameState());
			GameStateComponent->RegisterComponent();

			UStageComponent* StageComponent = GameStateComponent->GetStageComponent().Get();
			StageComponent->SetCurrentStage(PVPBattleStage::WaitingForStart);

			auto FreeGameMode = NewChildComponent<UFreeGameModeComponent>();
			FreeGameMode->RegisterComponent();
			FreeGameMode->Start();

			co_await OnGameStartConditionsMet;

			// 지난 매치의 소품들 관상용으로 지금까지 살려둔 것 제거
			DestroyComponentIfValid(PrevBattleGameModeComponent);
			DestroyComponentIfValid(PrevGameStateComponent);

			StageComponent->SetCurrentStage(PVPBattleStage::WillPlay);

			auto BattleGameMode = NewChildComponent<UBattleGameModeComponent>();
			BattleGameMode->RegisterComponent();
			// TODO real numbers
			BattleGameMode->Configure(2, 2);

			const float PlayStartTime = GetWorld()->GetTimeSeconds() + 5.f;
			StageComponent->SetStageWorldStartTime(PVPBattleStage::Playing, PlayStartTime);

			co_await WorldTimerComponent->At(PlayStartTime);
			StageComponent->SetCurrentStage(PVPBattleStage::Playing);

			FreeGameMode->DestroyComponent();
			auto BattleEnd = BattleGameMode->Start();
			GameStateComponent->SetBattleGameStateComponent(BattleGameMode->GetGameStateComponent());
			co_await BattleEnd;

			StageComponent->SetCurrentStage(PVPBattleStage::Result);
			StageComponent->SetStageWorldStartTime(PVPBattleStage::Result, GetWorld()->GetTimeSeconds());

			for (APlayerState* Each : GetWorld()->GetGameState()->PlayerArray)
			{
				Each->FindComponentByClass<UReadyStateComponent>()->SetbReady(false);
				Each->GetPlayerController()->ChangeState(NAME_Spectating);
				Each->GetPlayerController()->ClientGotoState(NAME_Spectating);
			}

			PrevBattleGameModeComponent = BattleGameMode;
			PrevGameStateComponent = GameStateComponent;
		}
	}
};
