// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"


/**
 * 코루틴에 co_await 기능을 제공하는 promise_type.
 * 코루틴 정의 시 promise_type에 이 클래스를 상속시켜 기능을 얻을 수 있습니다.
 *
 * @tparam Derived 이 클래스를 상속하는 promise_type (CRTP)
 */
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


/**
 * TAwaitablePromise를 상속하는 promise_type에 대한 코루틴이 상속하는 클래스
 *
 * @see TAwaitablePromise
 * @tparam Derived 이 클래스를 상속하는 return_object 클래스 (CRTP)
 * @tparam T co_await의 반환형
 */
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