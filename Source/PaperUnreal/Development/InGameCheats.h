// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
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
	void SomeCheat(double Value)
	{
	}
};
