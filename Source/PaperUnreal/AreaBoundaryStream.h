// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/LiveData.h"
#include "Core/SegmentArray.h"
#include "UObject/Interface.h"
#include "AreaBoundaryStream.generated.h"

UINTERFACE()
class UAreaBoundaryStream : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IAreaBoundaryStream
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<FLoopedSegmentArray2D> GetBoundaryStreamer() = 0;
};
