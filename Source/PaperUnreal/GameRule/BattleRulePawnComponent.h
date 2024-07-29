// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleRuleGameStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/AreaSlasherComponent.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerKillerComponent.h"
#include "PaperUnreal/AreaTracer/TracerPointEventComponent.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerToAreaConverterComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentAttacherComponent.h"
#include "PaperUnreal/ModeAgnostic/PawnSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/ModeAgnostic/LineMeshComponent.h"
#include "BattleRulePawnComponent.generated.h"


UCLASS(Within=Character2)
class UBattleRulePawnComponent : public UComponentAttacherComponent
{
	GENERATED_BODY()

public:
	void SetDependencies(
		UBattleRuleGameStateComponent* InGameState,
		AAreaActor* InHomeArea)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerGameState = InGameState;
		HomeArea = InHomeArea;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_HomeArea)
	AAreaActor* RepHomeArea;
	TLiveData<AAreaActor*&> HomeArea{RepHomeArea};

	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	TScriptInterface<ITracerPathProvider> RepTracerPathProvider;
	TLiveData<TScriptInterface<ITracerPathProvider>&> TracerPathProvider{RepTracerPathProvider};

	UFUNCTION()
	void OnRep_HomeArea() { HomeArea.Notify(); }

	UFUNCTION()
	void OnRep_TracerPathProvider() { TracerPathProvider.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepHomeArea, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPathProvider, COND_InitialOnly);
	}

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
			check(AllValid(ServerGameState, RepHomeArea));
		}
	}

	virtual void AttachServerMachineComponents() override
	{
		auto TracerPath = NewChildComponent<UTracerPathComponent>(GetOwner());
		TracerPath->SetNoPathArea(HomeArea.Get()->ServerAreaBoundary);
		TracerPath->RegisterComponent();
		TracerPathProvider = TScriptInterface<ITracerPathProvider>{TracerPath};

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewChildComponent<UReplicatedTracerPathComponent>(GetOwner());
			TracerPathReplicator->SetTracerPathSource(TracerPath);
			TracerPathReplicator->RegisterComponent();
			TracerPathProvider = TracerPathReplicator;
		}

		ServerOverlapChecker = NewChildComponent<UTracerOverlapCheckerComponent>(GetOwner());
		ServerOverlapChecker->SetTracer(TracerPath);
		ServerOverlapChecker->RegisterComponent();

		ServerGameState->ServerPawnSpawner->GetSpawnedPawns().ObserveAddIfValid(this, [this](APawn* NewPlayer)
		{
			auto NewPlayerHome = NewPlayer->FindComponentByClass<UBattleRulePawnComponent>()->HomeArea.Get();
			auto NewPlayerTracer = NewPlayer->FindComponentByClass<UTracerPathComponent>();

			if (NewPlayerHome != HomeArea.Get())
			{
				ServerOverlapChecker->AddOverlapTarget(NewPlayerTracer);
			}
		});

		ServerTracerToAreaConverter = NewChildComponent<UTracerToAreaConverterComponent>(GetOwner());
		ServerTracerToAreaConverter->SetTracer(TracerPath);
		ServerTracerToAreaConverter->SetConversionDestination(HomeArea.Get()->ServerAreaBoundary);
		ServerTracerToAreaConverter->RegisterComponent();

		ServerGameState->ServerAreaSpawner->GetSpawnedAreas().ObserveAddIfValid(this, [this](AAreaActor* NewArea)
		{
			if (NewArea != HomeArea.Get())
			{
				auto AreaSlasher = NewChildComponent<UAreaSlasherComponent>(GetOwner());
				AreaSlasher->SetSlashTarget(NewArea->ServerAreaBoundary);
				AreaSlasher->SetTracerToAreaConverter(ServerTracerToAreaConverter);
				AreaSlasher->RegisterComponent();
			}
		});

		auto Killer = NewChildComponent<UTracerKillerComponent>(GetOwner());
		Killer->SetTracer(TracerPath);
		Killer->SetArea(HomeArea.Get()->ServerAreaBoundary);
		Killer->SetOverlapChecker(ServerOverlapChecker);
		Killer->RegisterComponent();

		auto Life = NewChildComponent<ULifeComponent>(GetOwner());
		Life->RegisterComponent();

		RunWeakCoroutine(this, [this, Life](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await Life->GetbAlive().If(false);

			// 현재 얘만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라 알아서 사라짐
			// Path를 파괴해서 상호작용을 없애 게임에 영향을 미치지 않게 한다
			FindAndDestroyComponent<UTracerPathComponent>(GetOwner());
		});
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			co_await HomeArea;
			co_await TracerPathProvider;

			ClientTracerMesh = NewChildComponent<ULineMeshComponent>(GetOwner());
			ClientTracerMesh->RegisterComponent();
			
			auto TracerPointEvent = NewChildComponent<UTracerPointEventComponent>(GetOwner());
			TracerPointEvent->SetEventSource(TracerPathProvider.Get().GetInterface());
			TracerPointEvent->RegisterComponent();
			TracerPointEvent->AddEventListener(ClientTracerMesh);

			// 일단 위에까지 완료했으면 플레이는 가능한 거임 여기부터는 미적인 요소들을 준비한다
			auto PlayerState = co_await GetOuterACharacter2()->WaitForPlayerState();
			auto Inventory = co_await WaitForComponent<UInventoryComponent>(PlayerState);
			
			RunWeakCoroutine(this, [this, &Inventory](FWeakCoroutineContext&) -> FWeakCoroutine
			{
				for (auto MaterialStream = Inventory->GetTracerMaterial().CreateStream();;)
				{
					auto SoftMaterial = co_await Filter(MaterialStream, [](const auto& Soft) { return !Soft.IsNull(); });
					auto Material = co_await RequestAsyncLoad(SoftMaterial);
					ClientTracerMesh->ConfigureMaterialSet({Material});
				}
			});
		});
	}
};
