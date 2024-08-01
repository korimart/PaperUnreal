// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "AreaMeshGeneratorComponent.h"
#include "ReplicatedAreaBoundaryComponent.h"
#include "PaperUnreal/GameFramework2/Actor2.h"
#include "PaperUnreal/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/ModeAgnostic/SolidColorMaterial.h"
#include "PaperUnreal/ModeAgnostic/TeamComponent.h"
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

private:
	UPROPERTY(ReplicatedUsing=OnRep_AreaBaseColor)
	FLinearColor RepAreaBaseColor;
	mutable TLiveData<FLinearColor&> AreaBaseColor{RepAreaBaseColor};

	UFUNCTION()
	void OnRep_AreaBaseColor() { AreaBaseColor.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepAreaBaseColor);
	}

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
			auto ColorStream = AreaBaseColor.CreateStream();
			auto FirstColor = co_await ColorStream;
			auto Material = co_await FSolidColorMaterial::Create(FirstColor);

			ClientAreaMesh->ConfigureMaterialSet({Material.Get()});

			while (true)
			{
				Material.SetColor(co_await ColorStream);
			}
		});
	    
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await LifeComponent->GetbAlive().If(false);

			// TODO play death animation
		});
	}
};
