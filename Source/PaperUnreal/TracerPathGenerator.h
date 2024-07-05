// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "UObject/Interface.h"
#include "TracerPathGenerator.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTracerPathGenerator : public UInterface
{
	GENERATED_BODY()
};


UENUM()
enum class ETracerPathEvent
{
	GenerationStarted,
	NewPointGenerated,
	LastPointModified,
	GenerationEnded,
};


struct FTracerPathEvent
{
	ETracerPathEvent Event;
	TOptional<FVector2D> Point;
	TOptional<FVector2D> PathDirection;

	bool GenerationStarted() const { return Event == ETracerPathEvent::GenerationStarted; }
	bool NewPointGenerated() const { return Event == ETracerPathEvent::NewPointGenerated; }
	bool LastPointModified() const { return Event == ETracerPathEvent::LastPointModified; }
	bool GenerationEnded() const { return Event == ETracerPathEvent::GenerationEnded; }
};


/**
 * 
 */
class ITracerPathGenerator
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPathEvent, FTracerPathEvent);
	FOnPathEvent OnPathEvent;

	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() = 0;
};
