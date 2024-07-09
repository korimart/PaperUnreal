// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ByteStreamComponent.h"
#include "UObject/Interface.h"
#include "TracerPathGenerator.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTracerPathStream : public UInterface
{
	GENERATED_BODY()
};


struct FTracerPathPoint
{
	static constexpr int32 ChunkSize = 32;

	FVector2D Point;
	FVector2D PathDirection;

	friend FArchive& operator<<(FArchive& Ar, FTracerPathPoint& PathPoint)
	{
		Ar << PathPoint.Point;
		Ar << PathPoint.PathDirection;
		return Ar;
	}
};


using FTracerPathEvent = TStreamEvent<FTracerPathPoint>;


/**
 * 
 */
class ITracerPathStream
{
	GENERATED_BODY()

public:
	virtual TValueStream<FTracerPathEvent> CreatePathStream() = 0;
};
