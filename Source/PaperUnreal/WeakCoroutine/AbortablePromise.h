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
		return TFunction<void()>{[This, bThisAlive = TWeakPtr<bool>{bAbortRequested}]()
		{
			if (bThisAlive.IsValid())
			{
				This->Abort();
			}
		}};
	}

private:
	TSharedRef<bool> bAbortRequested = MakeShared<bool>();
};


template <typename AbortablePromiseType, typename InnerAwaitableType>
class TAbortablePromiseAwaitable
{
public:
	template <typename AwaitableType>
	TAbortablePromiseAwaitable(AbortablePromiseType& AbortablePromise, AwaitableType&& Awaitable)
		: AbortablePromiseHandle(std::coroutine_handle<AbortablePromiseType>::from_promise(AbortablePromise))
		  , InnerAwaitable(Forward<AwaitableType>(Awaitable))
	{
	}

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	bool await_suspend(HandleType&& Handle)
	{
		if (AbortablePromiseHandle.promise().IsAbortRequested())
		{
			AbortablePromiseHandle.destroy();
			return true;
		}

		if (InnerAwaitable.await_ready())
		{
			return false;
		}

		struct FHandle
		{
			std::decay_t<HandleType> Handle;
			TAbortablePromiseAwaitable* This;

			void resume() const
			{
				This->AbortablePromiseHandle.promise().IsAbortRequested()
					? AbortablePromiseHandle.destroy()
					: Handle.resume();
			}

			void destroy() const
			{
				This->AbortablePromiseHandle.promise().IsAbortRequested()
					? AbortablePromiseHandle.destroy()
					: Handle.destroy();
			}
		};

		using SuspendType = decltype(InnerAwaitable.await_suspend(std::declval<FHandle>()));

		if constexpr (std::is_void_v<SuspendType>)
		{
			InnerAwaitable.await_suspend(FHandle{Forward<HandleType>(Handle), this});
			return true;
		}
		else
		{
			return InnerAwaitable.await_suspend(FHandle{Forward<HandleType>(Handle), this});
		}
	}

	auto await_resume()
	{
		return InnerAwaitable.await_resume();
	}

private:
	std::coroutine_handle<AbortablePromiseType> AbortablePromiseHandle;
	InnerAwaitableType InnerAwaitable;
};

template <typename AbortablePromiseType, typename AwaitableType>
TAbortablePromiseAwaitable(AbortablePromiseType&, AwaitableType&&) -> TAbortablePromiseAwaitable<AbortablePromiseType, AwaitableType>;
