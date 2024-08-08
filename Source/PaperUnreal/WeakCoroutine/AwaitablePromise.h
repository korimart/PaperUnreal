// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"


template <typename ReturnType>
class TAwaitablePromise
{
public:
	template <typename U>
	void return_value(U&& Value)
	{
		this->Promise.SetValue(Forward<U>(Value));
	}

	TCancellableFuture<ReturnType> GetFuture()
	{
		return Promise.GetFuture();
	}
	
private:
	TCancellablePromise<ReturnType> Promise;
};


template <>
class TAwaitablePromise<void>
{
public:
	void return_void()
	{
		this->Promise.SetValue();
	}
	
	TCancellableFuture<void> GetFuture()
	{
		return Promise.GetFuture();
	}
	
private:
	TCancellablePromise<void> Promise;
};


template <typename Derived, typename T>
class TAwaitableCoroutine : public TCancellableFutureAwaitable<TCancellableFuture<T>>
{
	using Super = TCancellableFutureAwaitable<TCancellableFuture<T>>;
	
public:
	template <typename PromiseType>
	TAwaitableCoroutine(std::coroutine_handle<PromiseType> Handle)
		: Super(static_cast<TAwaitablePromise<T>&>(Handle.promise()).GetFuture())
	{
	}

	void await_abort()
	{
		Super::await_abort();
		static_cast<Derived*>(this)->OnAwaitAbort();
	}

	template <typename FuncType>
	void Then(FuncType&& Func) const
	{
		this->Future.Then(Forward<FuncType>(Func));
	}
};