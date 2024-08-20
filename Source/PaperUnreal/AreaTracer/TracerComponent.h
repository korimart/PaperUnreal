// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReplicatedTracerPathComponent.h"
#include "TracerPathComponent.h"
#include "TracerPointEventComponent.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/GameMode/ModeAgnostic/AssetPaths.h"
#include "PaperUnreal/GameMode/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/GameMode/ModeAgnostic/LineMeshComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/SolidColorMaterial.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "TracerComponent.generated.h"


UCLASS(Within=Character2)
class UTracerComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	UPROPERTY()
	UTracerPathComponent* ClientPredictedTracerPath;

	void SetTracerColorStream(TValueStream<FLinearColor>&& Stream)
	{
		check(GetNetMode() != NM_DedicatedServer);
		ColorFeeder = InitiateColorFeeder(MoveTemp(Stream));
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	UReplicatedTracerPathComponent* RepTracerPath;
	TLiveData<UReplicatedTracerPathComponent*&> ReplicatedTracerPath{RepTracerPath};

	UFUNCTION()
	void OnRep_TracerPathProvider() { ReplicatedTracerPath.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPath, COND_InitialOnly);
	}

	UPROPERTY()
	ULineMeshComponent* ClientTracerMesh;

	UPROPERTY()
	FSolidColorMaterial ClientTracerMaterial;

	TAbortableCoroutineHandle<FWeakCoroutine> ColorFeeder;

	virtual void AttachServerMachineComponents() override
	{
		ServerTracerPath = NewChildComponent<UTracerPathComponent>(GetOwner());
		ServerTracerPath->RegisterComponent();

		if (GetNetMode() != NM_Standalone)
		{
			ReplicatedTracerPath = NewChildComponent<UReplicatedTracerPathComponent>(GetOwner());
			ReplicatedTracerPath->SetTracerPathSource(ServerTracerPath);
			ReplicatedTracerPath->RegisterComponent();
		}
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			// Standalone 또는 Listen Server에서는 PlayerMachine에서도 ServerTracerPath가 있을 수가 있음
			if (!IsValid(ServerTracerPath))
			{
				co_await ReplicatedTracerPath;

				ClientPredictedTracerPath = NewChildComponent<UTracerPathComponent>(GetOwner());
				ClientPredictedTracerPath->RegisterComponent();

				Cast<UCharacterMovementComponent2>(GetOuterACharacter2()->GetMovementComponent())
					->OnCorrected.AddWeakLambda(this, [this]()
					{
						ClientPredictedTracerPath->OverrideHeadAndTail(
							ReplicatedTracerPath.Get()->GetRunningPathHead().Get(),
							ReplicatedTracerPath.Get()->GetRunningPathTail().Get());
					});
			}

			ClientTracerMesh = NewChildComponent<ULineMeshComponent>(GetOwner());
			ClientTracerMesh->RegisterComponent();

			auto TracerPointEvent = NewChildComponent<UTracerPointEventComponent>(GetOwner());
			TracerPointEvent->SetEventSource(IsValid(ServerTracerPath) ? ServerTracerPath : ClientPredictedTracerPath);
			TracerPointEvent->RegisterComponent();
			TracerPointEvent->AddEventListener(ClientTracerMesh);

			auto BaseColorMaterial = co_await RequestAsyncLoad(FAssetPaths::SoftBaseColorMaterial());
			ClientTracerMaterial.SetBaseColorMaterial(BaseColorMaterial);
			ClientTracerMesh->ConfigureMaterialSet({ClientTracerMaterial.Get()});
		});
	}

	FWeakCoroutine InitiateColorFeeder(TValueStream<FLinearColor> ColorStream)
	{
		while (true)
		{
			ClientTracerMaterial.SetColor(co_await ColorStream);
		}
	}
};
