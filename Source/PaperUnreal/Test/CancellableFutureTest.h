// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/WeakCoroutine/ErrorReporting.h"
#include "CancellableFutureTest.generated.h"


UCLASS()
class UDummy : public UObject
{
	GENERATED_BODY()
};


UCLASS()
class UDummyError : public UFailableResultError
{
	GENERATED_BODY()
};


UCLASS()
class UDummyError2 : public UFailableResultError
{
	GENERATED_BODY()
};