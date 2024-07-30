// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleRuleGameStateComponent.h"
#include "PaperUnreal/AreaTracer/AreaSlasherComponent.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/AreaTracer/TracerKillerComponent.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerToAreaConverterComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/ModeAgnostic/CharacterMeshFromInventory.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/LineMeshComponent.h"
#include "BattleRulePawnComponent.generated.h"


UCLASS()
class UBattleRulePawnComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	ULifeComponent* GetServerLife() const
	{
		check(GetNetMode() != NM_Client);
		return RepLife;
	}

	void SetDependencies(
		UBattleRuleGameStateComponent* InGameState,
		AAreaActor* InHomeArea)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerGameState = InGameState;
		ServerHomeArea = InHomeArea;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	TScriptInterface<ITracerPathProvider> RepTracerPathProvider;
	TLiveData<TScriptInterface<ITracerPathProvider>&> TracerPathProvider{RepTracerPathProvider};

	UPROPERTY(ReplicatedUsing=OnRep_Life)
	ULifeComponent* RepLife;
	TLiveData<ULifeComponent*&> Life{RepLife};

	UFUNCTION()
	void OnRep_TracerPathProvider() { TracerPathProvider.Notify(); }

	UFUNCTION()
	void OnRep_Life() { Life.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPathProvider, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepLife, COND_InitialOnly);
	}

	UPROPERTY()
	AAreaActor* ServerHomeArea;

	UPROPERTY()
	UBattleRuleGameStateComponent* ServerGameState;

	UPROPERTY()
	UTracerOverlapCheckerComponent* ServerOverlapChecker;

	UPROPERTY()
	UTracerToAreaConverterComponent* ServerTracerToAreaConverter;

	UPROPERTY()
	ULineMeshComponent* ClientTracerMesh;

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			check(AllValid(ServerGameState, ServerHomeArea));
		}
	}

	virtual void AttachServerMachineComponents() override
	{
		auto Tracer = NewChildComponent<UTracerComponent>(GetOwner());
		Tracer->RegisterComponent();
		Tracer->ServerTracerPath->SetNoPathArea(ServerHomeArea->ServerAreaBoundary);

		ServerOverlapChecker = NewChildComponent<UTracerOverlapCheckerComponent>(GetOwner());
		ServerOverlapChecker->SetTracer(Tracer->ServerTracerPath);
		ServerOverlapChecker->RegisterComponent();

		ServerGameState->ServerPawnSpawner->GetSpawnedPawns().ObserveAddIfValid(this, [this](APawn* NewPlayer)
		{
			auto NewPlayerHome = NewPlayer->FindComponentByClass<UBattleRulePawnComponent>()->ServerHomeArea;
			auto NewPlayerTracer = NewPlayer->FindComponentByClass<UTracerPathComponent>();

			if (NewPlayerHome != ServerHomeArea)
			{
				ServerOverlapChecker->AddOverlapTarget(NewPlayerTracer);
			}
		});

		ServerTracerToAreaConverter = NewChildComponent<UTracerToAreaConverterComponent>(GetOwner());
		ServerTracerToAreaConverter->SetTracer(Tracer->ServerTracerPath);
		ServerTracerToAreaConverter->SetConversionDestination(ServerHomeArea->ServerAreaBoundary);
		ServerTracerToAreaConverter->RegisterComponent();

		ServerGameState->ServerAreaSpawner->GetSpawnedAreas().ObserveAddIfValid(this, [this](AAreaActor* NewArea)
		{
			if (NewArea != ServerHomeArea)
			{
				auto AreaSlasher = NewChildComponent<UAreaSlasherComponent>(GetOwner());
				AreaSlasher->SetSlashTarget(NewArea->ServerAreaBoundary);
				AreaSlasher->SetTracerToAreaConverter(ServerTracerToAreaConverter);
				AreaSlasher->RegisterComponent();
			}
		});

		auto Killer = NewChildComponent<UTracerKillerComponent>(GetOwner());
		Killer->SetTracer(Tracer->ServerTracerPath);
		Killer->SetArea(ServerHomeArea->ServerAreaBoundary);
		Killer->SetOverlapChecker(ServerOverlapChecker);
		Killer->RegisterComponent();

		Life = NewChildComponent<ULifeComponent>(GetOwner());
		Life.Get()->RegisterComponent();

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await Life.Get()->GetbAlive().If(false);

			// 현재 얘만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라 알아서 사라짐
			// Path를 파괴해서 상호작용을 없애 게임에 영향을 미치지 않게 한다
			FindAndDestroyComponent<UTracerPathComponent>(GetOwner());

			// 클라이언트에서 데스 애니메이션을 플레이하고 액터를 숨기기에 충분한 시간을 준다
			co_await WaitForSeconds(GetWorld(), 10.f);

			GetOwner()->Destroy();
		});
	}

	virtual void AttachPlayerMachineComponents() override
	{
		NewChildComponent<UCharacterMeshFromInventory>(GetOwner())->RegisterComponent();

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await Life;
			co_await Life.Get()->GetbAlive().If(false);
			// TODO play death animation and hide actor
		});
	}
};
