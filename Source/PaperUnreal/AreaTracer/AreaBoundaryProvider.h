﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SegmentArray.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "UObject/Interface.h"
#include "AreaBoundaryProvider.generated.h"

UINTERFACE()
class UAreaBoundaryProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IAreaBoundaryProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<FLoopedSegmentArray2D> GetBoundary() const = 0;
};
