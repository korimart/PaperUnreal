// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/ModeAgnostic/ByteStreamComponent.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
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


struct FTracerPathPoint2
{
	FVector2D Point;
	FVector2D PathDirection;

	friend bool operator==(const FTracerPathPoint2& Left, const FTracerPathPoint2& Right)
	{
		return Left.Point.Equals(Right.Point) && Left.PathDirection.Equals(Right.PathDirection);
	}
};


/**
 * 
 */
class ITracerPathStream
{
	GENERATED_BODY()

public:
	virtual const TValueStreamer<FTracerPathEvent>& GetTracerPathStreamer() const { throw 3; }
	virtual TLiveDataView<TOptional<FTracerPathPoint2>> GetRunningPathHead() const { throw 3; }
	virtual TLiveDataView<TArray<FTracerPathPoint2>> GetRunningPathTail() const { throw 3; }
};
