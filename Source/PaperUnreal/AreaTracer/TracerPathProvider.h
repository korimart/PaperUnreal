// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "UObject/Interface.h"
#include "TracerPathProvider.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTracerPathProvider : public UInterface
{
	GENERATED_BODY()
};


/**
 * 
 */
class ITracerPathProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TOptional<FVector2D>> GetRunningPathHead() const = 0;
	virtual TLiveDataView<TArray<FVector2D>> GetRunningPathTail() const = 0;
};
