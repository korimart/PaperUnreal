// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "LifeComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
#include "TeamComponent.h"
#include "Core/Actor2.h"
#include "AreaActor.generated.h"


UCLASS()
class AAreaActor : public AActor2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ULifeComponent* LifeComponent;

	UPROPERTY()
	UTeamComponent* TeamComponent;

	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(TSoftObjectPtr<UMaterialInstance>, AreaMaterial, RepAreaMaterial);

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaMaterial)
	TSoftObjectPtr<UMaterialInstance> RepAreaMaterial;

	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		LifeComponent = CreateDefaultSubobject<ULifeComponent>(TEXT("LifeComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}

	UFUNCTION()
	void OnRep_AreaMaterial() { AreaMaterial.OnRep(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepAreaMaterial);
	}

	virtual void PostInitializeComponents() override
	{
		Super::PostInitializeComponents();

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			co_await LifeComponent->WaitForDeath();

			// TODO play death animation

			Destroy();
		});
	}

	virtual void AttachServerMachineComponents() override
	{
		auto AreaBoundary = NewObject<UAreaBoundaryComponent>(this);
		AreaBoundary->ResetToStartingBoundary(GetActorLocation());
		AreaBoundary->GetBoundary().Observe(this, [this](const FLoopedSegmentArray2D& Boundary)
		{
			if (!Boundary.IsValid())
			{
				LifeComponent->SetbAlive(false);
			}
		});
		AreaBoundary->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			auto ReplicatedAreaBoundary = NewObject<UReplicatedAreaBoundaryComponent>(this);
			ReplicatedAreaBoundary->SetAreaBoundarySource(AreaBoundary);
			ReplicatedAreaBoundary->RegisterComponent();
		}
	}

	virtual void AttachPlayerMachineComponents() override
	{
		auto AreaMesh = NewObject<UAreaMeshComponent>(this);
		AreaMesh->RegisterComponent();

		IAreaBoundaryProvider* AreaBoundaryStream = GetNetMode() == NM_Client
			? static_cast<IAreaBoundaryProvider*>(FindComponentByClass<UReplicatedAreaBoundaryComponent>())
			: static_cast<IAreaBoundaryProvider*>(FindComponentByClass<UAreaBoundaryComponent>());

		// 클래스 서버 코드에서 뭔가 실수한 거임 고쳐야 됨
		check(AreaBoundaryStream);

		auto AreaMeshGenerator = NewObject<UAreaMeshGeneratorComponent>(this);
		AreaMeshGenerator->SetMeshSource(AreaBoundaryStream);
		AreaMeshGenerator->SetMeshDestination(AreaMesh);
		AreaMeshGenerator->RegisterComponent();

		RunWeakCoroutine(this, [this, AreaMesh](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AbortIfNotValid(AreaMesh);
			for (auto AreaMaterialStream = AreaMaterial.CreateStream();;)
			{
				auto SoftAreaMaterial = co_await AbortOnError(AreaMaterialStream);
				AreaMesh->ConfigureMaterialSet({co_await AbortOnError(RequestAsyncLoad(SoftAreaMaterial))});
			}
		});
	}
};
