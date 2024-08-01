// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"


template <typename Derived>
struct TWeakPromise
{
	bool IsValid() const
	{
		return Algo::AllOf(WeakList, [](const auto& Each) { return Each.IsValid(); });
	}

	template <CUObjectUnsafeWrapper U>
	void AddToWeakList(const U& Weak)
	{
		WeakList.Emplace(TUObjectUnsafeWrapperTypeTraits<U>::GetUObject(Weak));
	}

	template <typename U>
	void RemoveFromWeakList(const U& Weak)
	{
		WeakList.Remove(TUObjectUnsafeWrapperTypeTraits<U>::GetUObject(Weak));
	}

private:
	template <typename, typename> friend class TWeakAwaitable;
	
	TArray<TWeakObjectPtr<const UObject>> WeakList;
	
	void OnWeakAwaitableNoResume()
	{
		static_cast<Derived*>(this)->OnAbortByInvalidity();
	}
};


template <typename PromiseType, typename InnerAwaitableType>
class TWeakAwaitable
	: public TConditionalResumeAwaitable<TWeakAwaitable<PromiseType, InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TWeakAwaitable, InnerAwaitableType>;
	using ReturnType = typename Super::ReturnType;

	template <typename AwaitableType>
	TWeakAwaitable(PromiseType& Promise, AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
			, WeakPromiseHandle(std::coroutine_handle<PromiseType>::from_promise(Promise))
	{
	}
	
	bool ShouldResume(const auto&...) const
	{
		if (!WeakPromiseHandle.promise().IsValid())
		{
			WeakPromiseHandle.promise().OnWeakAwaitableNoResume();
			return false;
		}
		
		return true;
	}
	
private:
	std::coroutine_handle<PromiseType> WeakPromiseHandle;
};

template <typename PromiseType, typename AwaitableType>
TWeakAwaitable(PromiseType& InHandle, AwaitableType&& Awaitable) -> TWeakAwaitable<PromiseType, AwaitableType>;
