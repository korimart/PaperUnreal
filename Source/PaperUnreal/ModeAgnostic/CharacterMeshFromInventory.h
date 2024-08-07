// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ComponentRegistry.h"
#include "InventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerState.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "CharacterMeshFromInventory.generated.h"


UCLASS(Within=Character2)
class UCharacterMeshFromInventory : public UActorComponent2
{
	GENERATED_BODY()

private:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto PlayerState = co_await GetOuterACharacter2()->WaitForPlayerState();
			auto Inventory = co_await WaitForComponent<UInventoryComponent>(PlayerState);
			
			auto NonNullStream = Inventory->GetCharacterMesh().CreateStream()
				| Awaitables::Filter([](const auto& SoftMesh) { return !SoftMesh.IsNull(); });

			while (true)
			{
				auto SoftMesh = co_await NonNullStream;
				auto Mesh = co_await RequestAsyncLoad(SoftMesh);
				GetOuterACharacter2()->GetMesh()->SetSkeletalMesh(Mesh);
			}
		});
	}
};
