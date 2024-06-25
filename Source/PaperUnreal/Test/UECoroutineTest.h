// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/UECoroutine.h"
#include "UECoroutineTest.generated.h"


class FUECoroutineCheckDestroy
{
public:
	FUECoroutineCheckDestroy() = default;
	
	~FUECoroutineCheckDestroy()
	{
		UE_LOG(LogTemp, Warning, TEXT("Destoryed"));
	}
};


UCLASS()
class UUECoroutineTestObject : public UObject
{
	GENERATED_BODY()

public:
	TSharedPtr<FSimpleProxy> Proxy;
	
	void DoSomeCoroutine()
	{
		RunWeakCoroutine(this, [this, SomeObject = MakeUnique<FUECoroutineCheckDestroy>()]() -> FWeakCoroutine
		{
			UE_LOG(LogTemp, Warning, TEXT("Hi"));
			int32 Value = co_await WaitForValue();
			UE_LOG(LogTemp, Warning, TEXT("Got %d"), Value);
			
			UE_LOG(LogTemp, Warning, TEXT("returning"));
			co_return;
		});
	}

	FSimpleAwaitable WaitForValue()
	{
		FSimpleAwaitable Ret;
		Proxy = Ret.Proxy;
		return Ret;
	}
};
