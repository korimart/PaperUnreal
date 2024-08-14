// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PVPBattlePlayerController.h"
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
	
	FSimpleMulticastDelegate OnGameStartConditionsMet;

	virtual void AttachServerMachineComponents() override
	{
		GameStateComponent = NewChildComponent<UPVPBattleGameStateComponent>();
	}

	bool StartGameIfConditionsMet()
	{
		const int32 ReadyCount = Algo::TransformAccumulate(
			::GetComponents<UReadyStateComponent>(GetGameState()->GetPlayerStateArray().Get()),
			[](auto Each) { return Each->GetbReady().Get() ? 1 : 0; },
			0);

		if (ReadyCount >= 2)
		{
			OnGameStartConditionsMet.Broadcast();
			return true;
		}

		return false;
	}

	FWeakCoroutine InitPrivilegeComponentOfNewPlayers()
	{
		auto AddPrivilegeComponents = [this](UPrivilegeComponent* Target)
		{
			Target->AddComponentForPrivilege(PVPBattlePrivilege::Host, UBattleConfigComponent::StaticClass());
			Target->AddComponentForPrivilege(PVPBattlePrivilege::Host, UGameStarterComponent::StaticClass(),
				FComponentInitializer::CreateWeakLambda(this, [this](UActorComponent* Component)
				{
					Cast<UGameStarterComponent>(Component)
						->ServerGameStarter.BindUObject(this, &ThisClass::StartGameIfConditionsMet);
				}));
			Target->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UCharacterSetterComponent::StaticClass());
			Target->AddComponentForPrivilege(PVPBattlePrivilege::Normie, UReadySetterComponent::StaticClass());
			return Target;
		};

		auto PrivilegeStream
			= GetGameState()->GetPlayerStateArray().CreateAddStream()
			| Awaitables::Transform(&APlayerState::GetPlayerController)
			| Awaitables::FindComponentByClass<UPrivilegeComponent>()
			| Awaitables::Transform([&](UPrivilegeComponent* Component) { return AddPrivilegeComponents(Component); });

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
			FreeGameMode->Start(GetDefaultPawnClass());
			auto F = FinallyIfValid(FreeGameMode, [FreeGameMode]() { FreeGameMode->DestroyComponent(); });

			co_await OnGameStartConditionsMet;

			StageComponent->SetCurrentStage(PVPBattleStage::WillPlay);

			const float PlayStartTime = GetWorld()->GetTimeSeconds() + 5.f;
			StageComponent->SetStageWorldStartTime(PVPBattleStage::Playing, PlayStartTime);

			co_await GameStateComponent->WorldTimerComponent->At(PlayStartTime);
			StageComponent->SetCurrentStage(PVPBattleStage::Playing);
		}

		auto ResultComponent = co_await [&]()
		{
			auto BattleGameMode = NewChildComponent<UBattleGameModeComponent>();
			BattleGameMode->RegisterComponent();
			return BattleGameMode->Start(GetDefaultPawnClass(), 2, 2);
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
