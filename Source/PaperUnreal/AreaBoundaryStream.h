﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Core/UECoroutine.h"
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
	virtual TValueStream<FLoopedSegmentArray2D> CreateBoundaryStream() = 0;
};
