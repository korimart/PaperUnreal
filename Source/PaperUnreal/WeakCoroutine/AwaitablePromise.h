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


template <typename T>
class TAwaitableCoroutine
{
public:
	template <typename PromiseType>
	TAwaitableCoroutine(std::coroutine_handle<PromiseType> Handle)
		: Return(static_cast<TAwaitablePromise<T>&>(Handle.promise()).GetFuture())
	{
	}
	
	TCancellableFuture<T> ReturnValue()
	{
		return MoveTemp(Return);
	}

	friend auto operator co_await(TAwaitableCoroutine& Coroutine)
	{
		return operator co_await(Coroutine.ReturnValue());
	}

	friend auto operator co_await(TAwaitableCoroutine&& Coroutine)
	{
		return operator co_await(Coroutine.ReturnValue());
	}
	
private:
	TCancellableFuture<T> Return;
};