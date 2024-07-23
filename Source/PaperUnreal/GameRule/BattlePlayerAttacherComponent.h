// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/AreaTracer/AreaSlasherComponent.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerKillerComponent.h"
#include "PaperUnreal/AreaTracer/TracerMeshComponent.h"
#include "PaperUnreal/AreaTracer/TracerMeshGeneratorComponent.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerToAreaConverterComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentAttacherComponent.h"
#include "PaperUnreal/ModeAgnostic/PlayerSpawnerComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "BattlePlayerAttacherComponent.generated.h"


UCLASS(Within=Character2)
class UBattlePlayerAttacherComponent : public UComponentAttacherComponent
{
	GENERATED_BODY()

public:
	void SetDependencies(
		UAreaSpawnerComponent* AreaSpawner,
		UPlayerSpawnerComponent* PlayerSpawner,
		AAreaActor* InHomeArea)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerAreaSpawner = AreaSpawner;
		ServerPlayerSpawner = PlayerSpawner;
		HomeArea = InHomeArea;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_HomeArea)
	AAreaActor* RepHomeArea;
	TBackedLiveData<AAreaActor*> HomeArea{RepHomeArea};

	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	TScriptInterface<ITracerPathStream> RepTracerPathProvider;
	TBackedLiveData<TScriptInterface<ITracerPathStream>> TracerPathProvider{RepTracerPathProvider};

	UFUNCTION()
	void OnRep_HomeArea() { HomeArea.OnRep(); }

	UFUNCTION()
	void OnRep_TracerPathProvider() { TracerPathProvider.OnRep(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepHomeArea, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPathProvider, COND_InitialOnly);
	}

	UPROPERTY()
	UAreaSpawnerComponent* ServerAreaSpawner;

	UPROPERTY()
	UPlayerSpawnerComponent* ServerPlayerSpawner;

	UPROPERTY()
	UTracerOverlapCheckerComponent* ServerOverlapChecker;

	UPROPERTY()
	UTracerToAreaConverterComponent* ServerTracerToAreaConverter;

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			check(AllValid(ServerAreaSpawner, ServerPlayerSpawner, RepHomeArea));
		}
	}

	virtual void AttachServerMachineComponents() override
	{
		auto TracerPath = NewObject<UTracerPathComponent>(GetOwner());
		TracerPath->SetNoPathArea(HomeArea->ServerAreaBoundary);
		TracerPath->RegisterComponent();
		TracerPathProvider = TScriptInterface<ITracerPathStream>{TracerPath};

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewObject<UReplicatedTracerPathComponent>(GetOwner());
			TracerPathReplicator->SetTracerPathSource(TracerPath);
			TracerPathReplicator->RegisterComponent();
			TracerPathProvider = TracerPathReplicator;
		}

		ServerOverlapChecker = NewObject<UTracerOverlapCheckerComponent>(GetOwner());
		ServerOverlapChecker->SetTracer(TracerPath);
		ServerOverlapChecker->RegisterComponent();

		ServerPlayerSpawner->GetSpawnedPlayers().ObserveAdd(this, [this](APawn* NewPlayer)
		{
			if (IsValid(NewPlayer))
			{
				auto NewPlayerHome = NewPlayer->FindComponentByClass<UBattlePlayerAttacherComponent>()->HomeArea.Get();
				auto NewPlayerTracer = NewPlayer->FindComponentByClass<UTracerPathComponent>();

				if (NewPlayerHome != HomeArea.Get())
				{
					ServerOverlapChecker->AddOverlapTarget(NewPlayerTracer);
				}
			}
		});

		ServerTracerToAreaConverter = NewObject<UTracerToAreaConverterComponent>(GetOwner());
		ServerTracerToAreaConverter->SetTracer(TracerPath);
		ServerTracerToAreaConverter->SetConversionDestination(HomeArea->ServerAreaBoundary);
		ServerTracerToAreaConverter->RegisterComponent();

		ServerAreaSpawner->GetSpawnedAreas().ObserveAdd(this, [this](AAreaActor* NewArea)
		{
			if (IsValid(NewArea) && NewArea != HomeArea.Get())
			{
				auto AreaSlasher = NewObject<UAreaSlasherComponent>(GetOwner());
				AreaSlasher->SetSlashTarget(NewArea->ServerAreaBoundary);
				AreaSlasher->SetTracerToAreaConverter(ServerTracerToAreaConverter);
				AreaSlasher->RegisterComponent();
			}
		});

		auto Killer = NewObject<UTracerKillerComponent>(GetOwner());
		Killer->SetTracer(TracerPath);
		Killer->SetArea(HomeArea->ServerAreaBoundary);
		Killer->SetOverlapChecker(ServerOverlapChecker);
		Killer->RegisterComponent();

		auto Life = NewObject<ULifeComponent>(GetOwner());
		Life->RegisterComponent();

		RunWeakCoroutine(this, [this, Life](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await AbortOnError(Life->GetbAlive().If(false));

			// 현재 얘만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라 알아서 사라짐
			// Path를 파괴해서 상호작용을 없애 게임에 영향을 미치지 않게 한다
			FindAndDestroyComponent<UTracerPathComponent>(GetOwner());
		});
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			co_await AbortOnError(HomeArea);
			co_await AbortOnError(TracerPathProvider);

			auto TracerMesh = NewObject<UTracerMeshComponent>(GetOwner());
			TracerMesh->RegisterComponent();

			auto TracerMeshGenerator = NewObject<UTracerMeshGeneratorComponent>(GetOwner());
			TracerMeshGenerator->SetMeshSource(TracerPathProvider.GetInterface());
			TracerMeshGenerator->SetMeshDestination(TracerMesh);
			TracerMeshGenerator->SetMeshAttachmentTarget(HomeArea->ClientAreaMesh);
			TracerMeshGenerator->RegisterComponent();

			// 일단 위에까지 완료했으면 플레이는 가능한 거임 여기부터는 미적인 요소들을 준비한다
			auto PlayerState = co_await AbortOnError(GetOuterACharacter2()->WaitForPlayerState());
			auto Inventory = co_await AbortOnError(WaitForComponent<UInventoryComponent>(PlayerState));

			Inventory->GetTracerMaterial().Observe(TracerMesh, [this, TracerMesh](auto SoftMaterial)
			{
				if (!SoftMaterial.IsNull())
				{
					RunWeakCoroutine(TracerMesh, [TracerMesh, SoftMaterial](FWeakCoroutineContext&) -> FWeakCoroutine
					{
						auto Material = co_await AbortOnError(RequestAsyncLoad(SoftMaterial));
						TracerMesh->ConfigureMaterialSet({Material});
					});
				}
			});
		});
	}
};
