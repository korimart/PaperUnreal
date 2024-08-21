// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
#include "PaperUnreal/GameFramework2/Actor2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/AssetPaths.h"
#include "PaperUnreal/GameMode/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/SolidColorMaterial.h"
#include "PaperUnreal/GameMode/ModeAgnostic/TeamComponent.h"
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

	UPROPERTY()
	UAreaBoundaryComponent* ServerAreaBoundary;

	UPROPERTY()
	UAreaMeshComponent* ClientAreaMesh;

	DECLARE_LIVE_DATA_GETTER_SETTER(AreaBaseColor);
	DECLARE_LIVE_DATA_GETTER_SETTER(ServerCalculatedArea);

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaBaseColor)
	FLinearColor RepAreaBaseColor;
	TLiveData<FLinearColor&> AreaBaseColor{RepAreaBaseColor};

	UPROPERTY(ReplicatedUsing=OnRep_ServerCalculatedArea)
	float RepServerCalculatedArea;
	TLiveData<float&> ServerCalculatedArea{RepServerCalculatedArea};

	UFUNCTION()
	void OnRep_AreaBaseColor() { AreaBaseColor.Notify(); }

	UFUNCTION()
	void OnRep_ServerCalculatedArea() { ServerCalculatedArea.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepAreaBaseColor);
		DOREPLIFETIME(ThisClass, RepServerCalculatedArea);
	}

	UPROPERTY()
	FSolidColorMaterial ClientAreaMaterial;

	AAreaActor()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
		LifeComponent = CreateDefaultSubobject<ULifeComponent>(TEXT("LifeComponent"));
		TeamComponent = CreateDefaultSubobject<UTeamComponent>(TEXT("TeamComponent"));
	}

	virtual void AttachServerMachineComponents() override
	{
		ServerAreaBoundary = NewObject<UAreaBoundaryComponent>(this);
		ServerAreaBoundary->ResetToStartingBoundary(GetActorLocation());
		ServerAreaBoundary->GetBoundary().Observe(this, [this](const FLoopedSegmentArray2D& Boundary)
		{
			if (!Boundary.IsValid())
			{
				LifeComponent->SetbAlive(false);
			}
		});
		ServerAreaBoundary->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto LifeAndArea = Stream::Combine(
				LifeComponent->GetbAlive().MakeStream(),
				ServerAreaBoundary->GetBoundary().MakeStream() | Awaitables::Transform(&FLoopedSegmentArray2D::CalculateArea));

			while (true)
			{
				const auto [bAlive, Area] = co_await LifeAndArea;
				ServerCalculatedArea = bAlive ? Area : 0.f;
			}
		});

		if (GetNetMode() != NM_Standalone)
		{
			auto ReplicatedAreaBoundary = NewObject<UReplicatedAreaBoundaryComponent>(this);
			ReplicatedAreaBoundary->SetAreaBoundarySource(ServerAreaBoundary);
			ReplicatedAreaBoundary->RegisterComponent();
		}
	}

	virtual void AttachPlayerMachineComponents() override
	{
		ClientAreaMesh = NewObject<UAreaMeshComponent>(this);
		ClientAreaMesh->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto BaseColorMaterial = co_await RequestAsyncLoad(FAssetPaths::SoftBaseColorMaterial());
			ClientAreaMaterial.SetBaseColorMaterial(BaseColorMaterial);
			ClientAreaMesh->ConfigureMaterialSet({ClientAreaMaterial.Get()});

			for (auto ColorStream = AreaBaseColor.MakeStream();;)
			{
				ClientAreaMaterial.SetColor(co_await ColorStream);
			}
		});

		IAreaBoundaryProvider* AreaBoundaryProvider = GetNetMode() == NM_Client
			? static_cast<IAreaBoundaryProvider*>(FindComponentByClass<UReplicatedAreaBoundaryComponent>())
			: static_cast<IAreaBoundaryProvider*>(FindComponentByClass<UAreaBoundaryComponent>());

		// 클래스 서버 코드에서 뭔가 실수한 거임 고쳐야 됨
		check(AreaBoundaryProvider);

		auto AreaMeshGenerator = NewObject<UAreaMeshGeneratorComponent>(this);
		AreaMeshGenerator->SetMeshSource(AreaBoundaryProvider);
		AreaMeshGenerator->SetMeshDestination(ClientAreaMesh);
		AreaMeshGenerator->RegisterComponent();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await LifeComponent->GetbAlive().If(false);
			ClientAreaMesh->DestroyComponent();
		});
	}
};
