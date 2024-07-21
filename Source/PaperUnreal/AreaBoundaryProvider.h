// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/LiveData.h"
#include "Core/SegmentArray.h"
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
	virtual TLiveDataView<TLiveData<FLoopedSegmentArray2D>> GetBoundary() = 0;
};
