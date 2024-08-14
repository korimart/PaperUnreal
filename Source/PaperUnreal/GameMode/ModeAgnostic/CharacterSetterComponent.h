// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "CharacterSetterComponent.generated.h"


UCLASS(Within=PlayerController)
class UCharacterSetterComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> MostRecentChoice;
	
	UFUNCTION(Server, Reliable)
	void ServerEquipManny();
	
	UFUNCTION(Server, Reliable)
	void ServerEquipQuinn();
	
private:
	UCharacterSetterComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};