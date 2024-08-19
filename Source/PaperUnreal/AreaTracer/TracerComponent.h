// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReplicatedTracerPathComponent.h"
#include "TracerPathComponent.h"
#include "TracerPointEventComponent.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
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

	void SetTracerColorStream(TValueStream<FLinearColor>&& Stream)
	{
		check(GetNetMode() != NM_DedicatedServer);
		ColorFeeder = InitiateColorFeeder(MoveTemp(Stream));
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_TracerPathProvider)
	TScriptInterface<ITracerPathProvider> RepTracerPathProvider;
	TLiveData<TScriptInterface<ITracerPathProvider>&> TracerPathProvider{RepTracerPathProvider};

	UFUNCTION()
	void OnRep_TracerPathProvider() { TracerPathProvider.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME_CONDITION(ThisClass, RepTracerPathProvider, COND_InitialOnly);
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
		TracerPathProvider = TScriptInterface<ITracerPathProvider>{ServerTracerPath};

		if (GetNetMode() != NM_Standalone)
		{
			auto TracerPathReplicator = NewChildComponent<UReplicatedTracerPathComponent>(GetOwner());
			TracerPathReplicator->SetTracerPathSource(ServerTracerPath);
			TracerPathReplicator->RegisterComponent();
			TracerPathProvider = TracerPathReplicator;
		}
	}

	virtual void AttachPlayerMachineComponents() override
	{
		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			co_await TracerPathProvider;

			ClientTracerMesh = NewChildComponent<ULineMeshComponent>(GetOwner());
			ClientTracerMesh->RegisterComponent();

			auto TracerPointEvent = NewChildComponent<UTracerPointEventComponent>(GetOwner());
			TracerPointEvent->SetEventSource(
				// Standalone 또는 Listen Server에서는 PlayerMachine에서도 ServerTracerPath가 있을 수가 있음
				IsValid(ServerTracerPath) ? ServerTracerPath : TracerPathProvider.Get().GetInterface());
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
