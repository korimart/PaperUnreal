﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "InventoryComponent.generated.h"


UCLASS()
class UInventoryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_GETTER_SETTER(CharacterMesh);
	DECLARE_LIVE_DATA_GETTER_SETTER(TracerBaseColor);

private:
	UPROPERTY(ReplicatedUsing=OnRep_CharacterMesh)
	TSoftObjectPtr<USkeletalMesh> RepCharacterMesh;
	mutable TLiveData<TSoftObjectPtr<USkeletalMesh>&> CharacterMesh{RepCharacterMesh};
	
	UPROPERTY(ReplicatedUsing=OnRep_TracerBaseColor)
	FLinearColor RepTracerBaseColor;
	mutable TLiveData<FLinearColor&> TracerBaseColor{RepTracerBaseColor};

	UFUNCTION()
	void OnRep_CharacterMesh() { CharacterMesh.Notify(); }
	
	UFUNCTION()
	void OnRep_TracerBaseColor() { TracerBaseColor.Notify(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepCharacterMesh);
		DOREPLIFETIME(ThisClass, RepTracerBaseColor);
	}
	
	UInventoryComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};