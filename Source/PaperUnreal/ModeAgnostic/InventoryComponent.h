// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/AreaTracer/AreaActor.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "InventoryComponent.generated.h"


UCLASS()
class UInventoryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(AAreaActor*, HomeArea, RepHomeArea); // TODO move
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(TSoftObjectPtr<UMaterialInstance>, TracerMaterial, RepTracerMaterial);

private:
	UPROPERTY(ReplicatedUsing=OnRep_HomeArea)
	AAreaActor* RepHomeArea;

	UPROPERTY(ReplicatedUsing=OnRep_TracerMaterial)
	TSoftObjectPtr<UMaterialInstance> RepTracerMaterial;

	UFUNCTION()
	void OnRep_HomeArea() { HomeArea.Notify(); }

	UFUNCTION()
	void OnRep_TracerMaterial() { TracerMaterial.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepHomeArea);
		DOREPLIFETIME(ThisClass, RepTracerMaterial);
	}
	
	UInventoryComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};
