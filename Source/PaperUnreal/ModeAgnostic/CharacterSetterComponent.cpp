﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterSetterComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InventoryComponent.h"


void UCharacterSetterComponent::ServerEquipManny_Implementation()
{
	GetOuterAPlayerController()->PlayerState->FindComponentByClass<UInventoryComponent>()->SetCharacterMesh(
		TSoftObjectPtr<USkeletalMesh>
		{
			FSoftObjectPath{TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny'")}
		});
}

void UCharacterSetterComponent::ServerEquipQuinn_Implementation()
{
	GetOuterAPlayerController()->PlayerState->FindComponentByClass<UInventoryComponent>()->SetCharacterMesh(
		TSoftObjectPtr<USkeletalMesh>
		{
			FSoftObjectPath{TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn'")}
		});
}
