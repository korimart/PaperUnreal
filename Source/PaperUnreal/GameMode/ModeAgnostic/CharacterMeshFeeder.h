// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/Character2.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "CharacterMeshFeeder.generated.h"


UCLASS(Within=Character2)
class UCharacterMeshFeeder : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshStream(TValueStream<TSoftObjectPtr<USkeletalMesh>> Stream)
	{
		Coroutine = InitiateMeshSetterSequence(MoveTemp(Stream) | Awaitables::IfNotNull());
	}

private:
	TAbortableCoroutineHandle<FWeakCoroutine> Coroutine;

	FWeakCoroutine InitiateMeshSetterSequence(auto NonNullStream)
	{
		while (true)
		{
			auto SoftMesh = co_await NonNullStream;
			auto Mesh = co_await RequestAsyncLoad(SoftMesh);
			GetOuterACharacter2()->GetMesh()->SetSkeletalMesh(Mesh);
		}
	}
};
