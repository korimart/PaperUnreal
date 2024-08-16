// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleGameStateComponent.h"
#include "BattlePawnKillerComponent.h"
#include "BattlePlayerStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperUnreal/AreaTracer/AreaSlasherComponent.h"
#include "PaperUnreal/AreaTracer/AreaSpawnerComponent.h"
#include "PaperUnreal/AreaTracer/ReplicatedTracerPathComponent.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerToAreaConverterComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/AssetPaths.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterMeshFeeder.h"
#include "BattlePawnComponent.generated.h"


UCLASS(Within=Character2)
class UBattlePawnComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	TLiveDataView<ULifeComponent*&> GetLife() const
	{
		return Life;
	}

	void SetDependencies(
		UBattleGameStateComponent* InGameState,
		AAreaActor* InHomeArea)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerGameState = InGameState;
		ServerHomeArea = InHomeArea;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_Tracer)
	UTracerComponent* RepTracer;
	TLiveData<UTracerComponent*&> Tracer{RepTracer};

	UFUNCTION()
	void OnRep_Tracer() { Tracer.Notify(); }

	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	TScriptInterface<ITracerPathProvider> RepTracerPathProvider;
	TLiveData<TScriptInterface<ITracerPathProvider>&> TracerPathProvider{RepTracerPathProvider};

	UFUNCTION()
	void OnRep_TracerPathProvider() { TracerPathProvider.Notify(); }

	UPROPERTY(ReplicatedUsing=OnRep_Life)
	ULifeComponent* RepLife;
	mutable TLiveData<ULifeComponent*&> Life{RepLife};

	UFUNCTION()
	void OnRep_Life() { Life.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracer, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPathProvider, COND_InitialOnly);
		DOREPLIFETIME_CONDITION(ThisClass, RepLife, COND_InitialOnly);
	}

	UPROPERTY()
	AAreaActor* ServerHomeArea;

	UPROPERTY()
	UBattleGameStateComponent* ServerGameState;

	UPROPERTY()
	UTracerOverlapCheckerComponent* ServerOverlapChecker;

	UPROPERTY()
	UTracerToAreaConverterComponent* ServerTracerToAreaConverter;

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
		Tracer = NewChildComponent<UTracerComponent>(GetOwner());
		Tracer.Get()->RegisterComponent();
		Tracer.Get()->ServerTracerPath->SetNoPathArea(ServerHomeArea->ServerAreaBoundary);

		ServerOverlapChecker = NewChildComponent<UTracerOverlapCheckerComponent>(GetOwner());
		ServerOverlapChecker->SetTracer(Tracer.Get()->ServerTracerPath);
		ServerOverlapChecker->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await AddToWeakList(ServerOverlapChecker);
			co_await AddToWeakList(ServerHomeArea);

			auto PawnComponentStream
				= ServerGameState->ServerPawnSpawner->GetSpawnedPawns().CreateAddStream()
				| Awaitables::FindComponentByClass<UBattlePawnComponent>()
				| Awaitables::Filter([this](UBattlePawnComponent* PawnComponent)
				{
					return PawnComponent->GetLife().Get()->GetbAlive().Get()
						&& PawnComponent->ServerHomeArea != ServerHomeArea;
				});

			while (true)
			{
				auto NewPawnComponent = co_await PawnComponentStream;
				ServerOverlapChecker->AddOverlapTarget(NewPawnComponent->Tracer.Get()->ServerTracerPath);
			}
		});

		ServerTracerToAreaConverter = NewChildComponent<UTracerToAreaConverterComponent>(GetOwner());
		ServerTracerToAreaConverter->SetTracer(Tracer.Get()->ServerTracerPath);
		ServerTracerToAreaConverter->SetConversionDestination(ServerHomeArea->ServerAreaBoundary);
		ServerTracerToAreaConverter->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await AddToWeakList(ServerTracerToAreaConverter);
			co_await AddToWeakList(ServerHomeArea);

			auto AreaStream
				= ServerGameState->GetAreaSpawner().Get()->GetSpawnedAreas().CreateAddStream()
				| Awaitables::Filter([](AAreaActor* Area) { return Area->LifeComponent->GetbAlive().Get(); })
				| Awaitables::IfNot(ServerHomeArea);

			while (true)
			{
				auto NewArea = co_await AreaStream;
				auto AreaSlasher = NewChildComponent<UAreaSlasherComponent>(GetOwner());
				AreaSlasher->SetSlashTarget(NewArea->ServerAreaBoundary);
				AreaSlasher->SetTracerToAreaConverter(ServerTracerToAreaConverter);
				AreaSlasher->RegisterComponent();
			}
		});

		auto Killer = NewChildComponent<UBattlePawnKillerComponent>(GetOwner());
		Killer->SetTracer(Tracer.Get()->ServerTracerPath);
		Killer->SetArea(ServerHomeArea->ServerAreaBoundary);
		Killer->SetOverlapChecker(ServerOverlapChecker);
		Killer->RegisterComponent();

		Life = NewChildComponent<ULifeComponent>(GetOwner());
		Life.Get()->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await Life.Get()->GetbAlive().If(false);

			GetOuterACharacter2()->GetCharacterMovement()->DisableMovement();

			// 현재 얘만 파괴해주면 나머지 컴포넌트는 디펜던시가 사라짐에 따라 알아서 사라짐
			// Path를 파괴해서 상호작용을 없애 게임에 영향을 미치지 않게 한다
			Tracer.Get()->DestroyComponent();

			// 클라이언트에서 데스 애니메이션을 플레이하고 액터를 숨기기에 충분한 시간을 준다
			co_await WaitForSeconds(GetWorld(), 10.f);

			GetOwner()->Destroy();
		});
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto PlayerState = co_await GetOuterACharacter2()->WaitForPlayerState();
			auto PlayerStateComponent = co_await WaitForComponent<UBattlePlayerStateComponent>(PlayerState);
			auto Inventory = co_await PlayerStateComponent->GetInventoryComponent();

			auto MeshFeeder = NewChildComponent<UCharacterMeshFeeder>();
			MeshFeeder->SetMeshStream(Inventory->GetCharacterMesh().MakeStream());
			MeshFeeder->RegisterComponent();

			co_await Tracer;
			Tracer.Get()->SetTracerColorStream(Inventory->GetTracerBaseColor().MakeStream());
		});

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			TStrongObjectPtr ExplosionActorClass{(co_await RequestAsyncLoad(FAssetPaths::SoftExplosionActor())).Unsafe()};

			co_await Life;
			co_await Life.Get()->GetbAlive().If(false);

			GetOwner()->GetRootComponent()->SetVisibility(false, true);
			GetWorld()->SpawnActorAbsolute(ExplosionActorClass.Get(), GetOwner()->GetActorTransform());
		});
	}
};
