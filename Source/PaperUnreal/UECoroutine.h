// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>


struct FWeakCoroutine
{
	struct promise_type
	{
		TWeakObjectPtr<UObject> Lifetime;
		TSharedPtr<TUniqueFunction<FWeakCoroutine()>> LambdaCaptures;
		TSharedPtr<bool> Destroyed = MakeShared<bool>(false);
		
		FWeakCoroutine get_return_object()
		{
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}
		
		std::suspend_always initial_suspend() const
		{
			return {};
		}
		
		std::suspend_never final_suspend() const noexcept
		{
			*Destroyed = true;
			return {};
		}

		void return_void() const
		{
		}

		void unhandled_exception() const
		{
		}
	};

	void Init(UObject* Lifetime, TSharedPtr<TUniqueFunction<FWeakCoroutine()>> LambdaCaptures)
	{
		Handle.promise().Lifetime = Lifetime;
		Handle.promise().LambdaCaptures = LambdaCaptures;
		Handle.resume();
	}

private:
	std::coroutine_handle<promise_type> Handle;
	
	FWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle)
	{
	}
};


struct FSimpleProxy
{
	TOptional<int32> Value;
	TSharedPtr<bool> Destroyed;
	std::coroutine_handle<FWeakCoroutine::promise_type> Handle;

	void SetValue(int32 InValue)
	{
		Value = InValue;

		if (Handle)
		{
			if (!*Destroyed)
			{
				if (Handle.promise().Lifetime.IsValid())
				{
					Handle.resume();
				}
				else
				{
					*Destroyed = true;
					Handle.destroy();
				}
			}
		}
	}
};


struct FSimpleAwaitable
{
	TSharedPtr<FSimpleProxy> Proxy = MakeShared<FSimpleProxy>();

	bool await_ready() const
	{
		return !!Proxy->Value;
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutine::promise_type> Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("suspended"));
		Proxy->Destroyed = Handle.promise().Destroyed;
		Proxy->Handle = Handle;
	}

	int32 await_resume()
	{
		UE_LOG(LogTemp, Warning, TEXT("resumed"));
		return *Proxy->Value;
	}
};


template <typename FuncType>
void RunWeakCoroutine(UObject* Lifetime, FuncType&& Func)
{
	auto LambdaCaptures = MakeShared<TUniqueFunction<FWeakCoroutine()>>(Forward<FuncType>(Func));
	(*LambdaCaptures)().Init(Lifetime, LambdaCaptures);
}