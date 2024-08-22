// Fill out your copyright notice in the Description page of Project Settings.

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
	/**
	 * 현재 플레이어가 착용하고 있는 캐릭터 메시 TSoftObjectPtr<USkeletalMesh>
	 */
	DECLARE_LIVE_DATA_GETTER_SETTER(CharacterMesh);
	
	/**
	 * 현재 플레이어가 착용하고 있는 트레이서의 컬러 FLinearColor
	 */
	DECLARE_LIVE_DATA_GETTER_SETTER(TracerBaseColor);

private:
	UPROPERTY(ReplicatedUsing=OnRep_CharacterMesh)
	TSoftObjectPtr<USkeletalMesh> RepCharacterMesh;
	TLiveData<TSoftObjectPtr<USkeletalMesh>&> CharacterMesh{RepCharacterMesh};
	
	UPROPERTY(ReplicatedUsing=OnRep_TracerBaseColor)
	FLinearColor RepTracerBaseColor;
	TLiveData<FLinearColor&> TracerBaseColor{RepTracerBaseColor};

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
