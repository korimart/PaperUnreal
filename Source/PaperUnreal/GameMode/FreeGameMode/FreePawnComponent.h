// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/AreaTracer/TracerComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/CharacterMeshFromInventory.h"
#include "FreePawnComponent.generated.h"


UCLASS()
class UFreePawnComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

private:
	virtual void AttachServerMachineComponents() override
	{
		NewChildComponent<UTracerComponent>(GetOwner())->RegisterComponent();
	}

	virtual void AttachPlayerMachineComponents() override
	{
		NewChildComponent<UCharacterMeshFromInventory>(GetOwner())->RegisterComponent();
	}
};