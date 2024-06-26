﻿// Copyright Epic Games, Inc. All Rights Reserved.

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
	FWeakAwaitableInt32 FetchInt()
	{
		FWeakAwaitableInt32 Ret;
		Handles.Add(Ret.GetHandle());
		return Ret;
	}

	void IssueValue(int32 Value)
	{
		if (!Handles.IsEmpty())
		{
			auto Handle = MoveTemp(Handles[0]);
			Handles.RemoveAt(0);
			Handle.SetValue(Value);
		}
	}

	void ClearRequests()
	{
		Handles.Empty();
	}

private:
	TArray<FWeakAwaitableHandleInt32> Handles;
};
