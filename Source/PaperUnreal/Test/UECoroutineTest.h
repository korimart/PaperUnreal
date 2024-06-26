// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/UECoroutine.h"
#include "UECoroutineTest.generated.h"


struct FUECoroutineTestLifetime
{
	TSharedPtr<bool> bDestroyed = MakeShared<bool>(false);

	~FUECoroutineTestLifetime()
	{
		*bDestroyed = true;
	}
};


struct FUECoroutineTestIncompatibleAwaitable
{
	bool await_ready() const
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutine::promise_type> Handle)
	{
	}

	void await_resume()
	{
	}
};


UCLASS()
class UUECoroutineTestValueProvider : public UObject
{
	GENERATED_BODY()

public:
	FSimpleAwaitable FetchValue()
	{
		FSimpleAwaitable Ret;
		Proxies.Add(Ret.Proxy);
		return Ret;
	}

	void IssueValue(int32 Value)
	{
		if (!Proxies.IsEmpty())
		{
			const auto Proxy = Proxies[0];
			Proxies.RemoveAt(0);
			Proxy->SetValue(Value);
		}
	}

private:
	TArray<TSharedPtr<FSimpleProxy>> Proxies;
};
