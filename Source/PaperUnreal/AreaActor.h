﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
#include "TeamComponent.h"
#include "Core/Actor2.h"
#include "Core/ComponentRegistry.h"
#include "AreaActor.generated.h"


UCLASS()
class AAreaActor : public AActor2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTeamComponent* TeamComponent;

	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(TSoftObjectPtr<UMaterialInstance>, AreaMaterial);

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaMaterial)
	TSoftObjectPtr<UMaterialInstance> RepAreaMaterial;

	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}

	UFUNCTION()
	void OnRep_AreaMaterial() { AreaMaterial = RepAreaMaterial; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepAreaMaterial);
	}

	virtual void AttachServerMachineComponents() override
	{
		auto AreaBoundary = NewObject<UAreaBoundaryComponent>(this);
		AreaBoundary->ResetToStartingBoundary(GetActorLocation());
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

		IAreaBoundaryStream* AreaBoundaryStream = GetNetMode() == NM_Client
			? static_cast<IAreaBoundaryStream*>(FindComponentByClass<UReplicatedAreaBoundaryComponent>())
			: static_cast<IAreaBoundaryStream*>(FindComponentByClass<UAreaBoundaryComponent>());

		// 클래스 서버 코드에서 뭔가 실수한 거임 고쳐야 됨
		check(AreaBoundaryStream);

		auto AreaMeshGenerator = NewObject<UAreaMeshGeneratorComponent>(this);
		AreaMeshGenerator->SetMeshSource(AreaBoundaryStream);
		AreaMeshGenerator->SetMeshDestination(AreaMesh);
		AreaMeshGenerator->RegisterComponent();

		RunWeakCoroutine(this, [this, AreaMesh](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AddToWeakList(AreaMesh);
			for (auto AreaMaterialStream = AreaMaterial.CreateStream();;)
			{
				auto SoftAreaMaterial = co_await AreaMaterialStream.Next();
				AreaMesh->ConfigureMaterialSet({co_await RequestAsyncLoad(SoftAreaMaterial)});
			}
		});
	}
};
