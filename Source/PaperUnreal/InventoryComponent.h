// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "Core/ActorComponent2.h"
#include "InventoryComponent.generated.h"


UCLASS()
class UInventoryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(AAreaActor*, HomeArea); // TODO move
	DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(TSoftObjectPtr<UMaterialInstance>, TracerMaterial);

private:
	UPROPERTY(ReplicatedUsing=OnRep_HomeArea)
	AAreaActor* RepHomeArea;

	UPROPERTY(ReplicatedUsing=OnRep_TracerMaterial)
	TSoftObjectPtr<UMaterialInstance> RepTracerMaterial;

	UInventoryComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	UFUNCTION()
	void OnRep_HomeArea() { HomeArea = RepHomeArea; }

	UFUNCTION()
	void OnRep_TracerMaterial() { TracerMaterial = RepTracerMaterial; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepHomeArea);
		DOREPLIFETIME(ThisClass, RepTracerMaterial);
	}
};
