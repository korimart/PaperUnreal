// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaActor.h"
#include "EngineUtils.h"
#include "Core/ActorComponentEx.h"
#include "Core/UECoroutine.h"
#include "AreaSpawnerComponent.generated.h"


UCLASS()
class UAreaSpawnerComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	TWeakAwaitable<AAreaActor*> WaitForAreaBelongingTo(AController* Controller)
	{
		// TODO
		for (TActorIterator<AAreaActor> It{GetWorld()}; It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
};
