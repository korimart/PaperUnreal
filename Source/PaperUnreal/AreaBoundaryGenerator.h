// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Core/UECoroutine.h"
#include "UObject/Interface.h"
#include "AreaBoundaryGenerator.generated.h"

UINTERFACE()
class UAreaBoundaryGenerator : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IAreaBoundaryGenerator
{
	GENERATED_BODY()

public:
	virtual TValueGenerator<FLoopedSegmentArray2D> CreateBoundaryGenerator() = 0;
};
