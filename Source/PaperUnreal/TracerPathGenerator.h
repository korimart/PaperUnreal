// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ByteArrayReplicatorComponent.h"
#include "UObject/Interface.h"
#include "TracerPathGenerator.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTracerPathGenerator : public UInterface
{
	GENERATED_BODY()
};


struct FTracerPathPoint
{
	FVector2D Point;
	FVector2D PathDirection;

	friend FArchive& operator<<(FArchive& Ar, FTracerPathPoint& PathPoint)
	{
		Ar << PathPoint.Point;
		Ar << PathPoint.PathDirection;
		return Ar;
	}
};


using FTracerPathEvent = TArrayEvent<FTracerPathPoint>;


/**
 * 
 */
class ITracerPathGenerator
{
	GENERATED_BODY()

public:
	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() = 0;
};
