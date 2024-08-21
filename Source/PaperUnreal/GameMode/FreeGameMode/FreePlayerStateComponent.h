// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/InventoryComponent.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "FreePlayerStateComponent.generated.h"


UCLASS()
class UFreePlayerStateComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	TLiveDataView<UInventoryComponent*&> GetInventoryComponent() const
	{
		return InventoryComponent;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_InventoryComponent)
	UInventoryComponent* RepInventoryComponent;
	mutable TLiveData<UInventoryComponent*&> InventoryComponent{RepInventoryComponent};

	UFUNCTION()
	void OnRep_InventoryComponent() { InventoryComponent.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);

		DOREPLIFETIME_CONDITION(ThisClass, RepInventoryComponent, COND_InitialOnly);
	}

	virtual void AttachServerMachineComponents() override
	{
		InventoryComponent = NewChildComponent<UInventoryComponent>();
		InventoryComponent->RegisterComponent();
	}
};
