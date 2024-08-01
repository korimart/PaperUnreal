// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"


struct FAbortablePromiseHandle
{
	FAbortablePromiseHandle(TFunction<void()> InAborter)
		: Aborter(MoveTemp(InAborter))
	{
	}

	void Abort()
	{
		Aborter();
	}

private:
	TFunction<void()> Aborter;
};


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

	FAbortablePromiseHandle GetAbortableHandle() const
	{
		TAbortablePromise* This = const_cast<TAbortablePromise*>(this);
		return TFunction<void()>
		{
			[This, bThisAlive = TWeakPtr<bool>{bAbortRequested}]()
			{
				if (bThisAlive.IsValid())
				{
					This->Abort();
				}
			}
		};
	}

private:
	TSharedRef<bool> bAbortRequested = MakeShared<bool>();
};


template <typename AbortablePromiseType, typename InnerAwaitableType>
class TAbortablePromiseAwaitable
	: public TConditionalResumeAwaitable<TAbortablePromiseAwaitable<AbortablePromiseType, InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortablePromiseAwaitable, InnerAwaitableType>;
	using ReturnType = typename Super::ReturnType;

	template <typename AwaitableType>
	TAbortablePromiseAwaitable(AbortablePromiseType& AbortablePromise, AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
		  , AbortablePromiseHandle(std::coroutine_handle<AbortablePromiseType>::from_promise(AbortablePromise))
	{
	}

	bool ShouldResume(const auto&...) const
	{
		return !AbortablePromiseHandle.promise().IsAbortRequested();
	}

private:
	std::coroutine_handle<AbortablePromiseType> AbortablePromiseHandle;
};

template <typename AbortablePromiseType, typename AwaitableType>
TAbortablePromiseAwaitable(AbortablePromiseType&, AwaitableType&&) -> TAbortablePromiseAwaitable<AbortablePromiseType, AwaitableType>;
