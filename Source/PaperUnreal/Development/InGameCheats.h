// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/GameStateBase.h"
#include "PaperUnreal/Vector2DArrayReplicatorComponent.h"
#include "InGameCheats.generated.h"

/**
 * 
 */
UCLASS()
class UInGameCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

private:
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SendLargeData(double Value)
	{
		TArray<FVector2D> Arr;
		for (int32 i = 0; i < 250; i++)
		{
			Arr.Add({Value, Value});
		}
		
		auto Component = GetWorld()->GetGameState()->FindComponentByClass<UVector2DArrayReplicatorComponent>();
		Component->ResetArray(Arr);
	}
};
