// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"


template <typename Derived>
struct TAbortablePromise
{
	void Abort()
	{
		if (!*bAbortRequested)
		{
			*bAbortRequested = true;
			static_cast<Derived*>(this)->OnAbortRequested();
		}
	}

	bool IsAbortRequested() const
	{
		return *bAbortRequested;
	}

private:
	TSharedRef<bool> bAbortRequested = MakeShared<bool>();
};


template <typename InnerAwaitableType>
class TAbortIfRequestedAwaitable
	: public TConditionalResumeAwaitable<TAbortIfRequestedAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortIfRequestedAwaitable, InnerAwaitableType>;
	using ReturnType = typename Super::ReturnType;

	template <typename AwaitableType>
	TAbortIfRequestedAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
	}
	
	bool await_suspend(const auto& AbortablePromiseHandle)
	{
		if (AbortablePromiseHandle.promise().IsAbortRequested())
		{
			AbortablePromiseHandle.destroy();
			return true;
		}
		
		return Super::await_suspend(AbortablePromiseHandle);
	}

	bool ShouldResume(const auto& AbortablePromiseHandle, const auto&) const
	{
		return !AbortablePromiseHandle.promise().IsAbortRequested();
	}
};

template <typename AwaitableType>
TAbortIfRequestedAwaitable(AwaitableType&&) -> TAbortIfRequestedAwaitable<AwaitableType>;


struct FAbortIfRequestedAdaptor : TAwaitableAdaptorBase<FAbortIfRequestedAdaptor>
{
	template <typename AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, FAbortIfRequestedAdaptor)
	{
		return TAbortIfRequestedAwaitable{Forward<AwaitableType>(Awaitable)};
	}
};


namespace Awaitables
{
	inline FAbortIfRequestedAdaptor AbortIfRequested()
	{
		return {};
	}
}
