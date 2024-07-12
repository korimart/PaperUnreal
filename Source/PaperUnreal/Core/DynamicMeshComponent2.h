// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshComponent2.generated.h"


UCLASS()
class UDynamicMeshComponent2 : public UDynamicMeshComponent
{
	GENERATED_BODY()

public:
	using UDynamicMeshComponent::NotifyMaterialSetUpdated;
};
