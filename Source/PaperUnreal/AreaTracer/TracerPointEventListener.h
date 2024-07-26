// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TracerPointEventListener.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTracerPointEventListener : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ITracerPointEventListener
{
	GENERATED_BODY()

public:
	virtual void OnTracerBegin() {}
	virtual void AddPoint(const FVector2D& Point) = 0;
	virtual void SetPoint(int32 Index, const FVector2D& Point) = 0;
	virtual void OnTracerEnd() {}
};
