﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReplicatedTracerPathComponent.h"
#include "TracerPathComponent.h"
#include "TracerPointEventComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/ModeAgnostic/ComponentAttacherComponent.h"
#include "PaperUnreal/ModeAgnostic/ComponentRegistry.h"
#include "PaperUnreal/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/ModeAgnostic/LineMeshComponent.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "TracerComponent.generated.h"


UCLASS(Within=Character2)
class UTracerComponent : public UComponentAttacherComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

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
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
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