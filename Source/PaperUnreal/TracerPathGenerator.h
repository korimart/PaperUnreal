// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/SegmentArray.h"
#include "Core/Utils.h"
#include "UObject/Interface.h"
#include "TracerPathGenerator.generated.h"

UINTERFACE()
class UTracerPathGenerator : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ITracerPathGenerator
{
	GENERATED_BODY()

public:
	virtual FSimpleMulticastDelegate& GetOnNewPointGenerated() = 0;
	virtual FSimpleMulticastDelegate& GetOnLastPointModified() = 0;
	virtual FSimpleMulticastDelegate& GetOnGenerationEnded() = 0;
	virtual const FSegmentArray2D& GetPath() const = 0;
};
