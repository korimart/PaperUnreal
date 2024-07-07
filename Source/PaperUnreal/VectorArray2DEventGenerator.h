// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UECoroutine.h"
#include "UObject/Interface.h"
#include "VectorArray2DEventGenerator.generated.h"

UINTERFACE()
class UVectorArray2DEventGenerator : public UInterface
{
	GENERATED_BODY()
};


enum class EVector2DArrayEvent
{
	Reset,
	Appended,
	LastModified,
};


struct FVector2DArrayEvent
{
	EVector2DArrayEvent Event;
	TArray<FVector2D> Affected;
};


/**
 * 
 */
class IVectorArray2DEventGenerator
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvent, FVector2DArrayEvent);
	FOnEvent OnEvent;

	virtual TValueGenerator<FVector2DArrayEvent> CreateEventGenerator() = 0;
};
