// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "ConditionalResumeAwaitable.h"


/**
 * 코루틴에 Abort 기능을 제공하는 promise_type.
 * 코루틴 정의 시 promise_type에 이 클래스를 상속시켜 기능을 얻을 수 있습니다.
 *
 * 코루틴에 Abort를 호출하면 코루틴은 이미 co_await 중인 경우에는 즉시, 아닌 경우에는 다음 co_await 진입 시 즉시 종료됩니다.
 * 이 코루틴 내에서 co_await 되는 Awaitable들은 모두 await_abort 함수를 추가적으로 구현해야 합니다.
 * Awaitable이 co_await 되고 있는 동안 코루틴에 Abort가 호출되면 Awaitable::await_abort가 호출됩니다.
 * 이 함수의 호출 이후 Awaitable은 더 이상 coroutine_handle에 접근해서는 안 됩니다. (resume, destroy를 호출하는 등)
 *
 * 중요 : TAbortablePromise를 상속하는 promise_type은 await_transform에서 Awaitable에 대해 Awaitables::AbortIfRequested()를 호출해야합니다.
 * 
 * @tparam Derived 이 클래스를 상속하는 promise_type (CRTP)
 */
template <typename Derived>
struct TAbortablePromise
{
	void Abort()
	{
		if (!bAbortRequested)
		{
			bAbortRequested = true;
			static_cast<Derived*>(this)->OnAbortRequested();
		}

		if (bSuspendedOnAbortAwaitable)
		{
			std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this)).destroy();
		}
	}

	bool IsAbortRequested() const
	{
		return bAbortRequested;
	}

private:
	template <typename>
	friend class TDestroyIfAbortRequestedAwaitable;

	bool bAbortRequested = false;
	bool bSuspendedOnAbortAwaitable = false;

	TWeakPtr<std::monostate> GetPromiseLifeFromDerived() const
	{
		return static_cast<const Derived*>(this)->PromiseLife;
	}
};


/**
 * TAbortablePromise를 상속하는 promise_type에 대한 코루틴이 상속하는 클래스
 *
 * @see TAbortablePromise
 * @tparam Derived 이 클래스를 상속하는 return_object 클래스 (CRTP)
 */
template <typename Derived>
class TAbortableCoroutine
{
public:
	/**
	 * 가장 빠른 기회에 코루틴을 중지하고 파괴합니다.
	 * 현재 co_await 중인 경우에는 즉시, 그 외에는 다음 co_await에서 Abort합니다.
	 */
	void Abort()
	{
		if (GetPromiseLife().IsValid())
		{
			GetHandle().promise().Abort();
		}
	}

	/**
	 * 코루틴이 이미 파괴되었는지 또는 파괴 예정인지 여부를 반환합니다.
	 */
	bool IsDeadMan() const
	{
		if (!GetPromiseLife().IsValid())
		{
			return true;
		}

		if (GetHandle().promise().IsAbortRequested())
		{
			return true;
		}

		return false;
	}

private:
	TWeakPtr<std::monostate> GetPromiseLife() const
	{
		return static_cast<const Derived*>(this)->PromiseLife;
	}

	auto GetHandle() const
	{
		return static_cast<const Derived*>(this)->Handle;
	}
};


/**
 * TAbortableCoroutine에 대한 Scoped Handle
 * 이 객체의 생명주기가 끝날 때 TAbortableCoroutine도 Abort합니다.
 * 
 * @tparam AbortableCoroutineType TAbortableCoroutine을 상속하는 코루틴 return_object 타입
 */
template <typename AbortableCoroutineType>
class TAbortableCoroutineHandle
{
public:
	TAbortableCoroutineHandle() = default;
	~TAbortableCoroutineHandle() { Reset(); }
	
	TAbortableCoroutineHandle(TAbortableCoroutineHandle&& Other)
		: Coroutine(MoveTemp(Other).Coroutine)
	{
		Other.Coroutine.Reset();
	}
	
	TAbortableCoroutineHandle& operator=(TAbortableCoroutineHandle&&) = delete;

	TAbortableCoroutineHandle(AbortableCoroutineType&& InCoroutine)
		: Coroutine(MoveTemp(InCoroutine))
	{
	}
	
	TAbortableCoroutineHandle& operator=(AbortableCoroutineType&& InCoroutine)
	{
		Reset();

		// Reset의 호출이 콜백을 불러서 여기 두 번 들어온 경우 Reset 이후에도 Set일 수 있음
		// (operator=의 호출이 operator=를 호출하는 circular 관계라는 뜻)
		check(!Coroutine);
		
		Coroutine.Emplace(MoveTemp(InCoroutine));
		return *this;
	}

	operator bool() const
	{
		return Coroutine && !Coroutine->IsDeadMan();
	}

	AbortableCoroutineType* operator->()
	{
		return &*Coroutine;
	}

	/**
	 * TArray<TAbortableCoroutineHandle<T>>에 TAbortableCoroutineHandle<T>를 추가하기 위한 편의 함수
	 * 배열에 핸들을 모아두고 나중에 일괄적으로 파괴하고 싶은 경우에 유용합니다.
	 *
	 * Array << DoSomething();
	 * Array << DoSomething();
	 * Array << DoSomething();
	 * 
	 * Array.Empty() // 세 코루틴을 일괄적으로 Abort
	 */
	friend void operator<<(TArray<TAbortableCoroutineHandle>& Array, TAbortableCoroutineHandle&& Right)
	{
		Array.Add(MoveTemp(Right));
	}

	/**
	 * 현재 가지고 있는 코루틴을 파괴합니다.
	 */
	void Reset()
	{
		if (Coroutine)
		{
			auto Copy = MoveTemp(Coroutine);
			Coroutine.Reset();

			// 이 호출로 인해 콜백이 실행되면서 다시 여기 들어오는 것을 방지하기 위해
			// 미리 Coroutine을 Reset하고 호출함
			Copy->Abort();
		}
	}
	
	AbortableCoroutineType& Get()
	{
		return *Coroutine;
	}
	
	const AbortableCoroutineType& Get() const
	{
		return *Coroutine;
	}

private:
	TOptional<AbortableCoroutineType> Coroutine;
};


/**
 * TAbortablePromise에서 기능의 구현을 위해 내부적으로 사용하는 Awaitable
 * TAbortablePromise인 코루틴에서 co_await를 호출하면 이 Awaitable로 Wrapping해
 * co_await 되고 있는 Awaitable이 resume 또는 destroy하지 않더라도 임의의 시점에 코루틴을 파괴할 수 있게 합니다.
 */
template <typename InnerAwaitableType>
class TDestroyIfAbortRequestedAwaitable
	: public TConditionalResumeAwaitable<TDestroyIfAbortRequestedAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TDestroyIfAbortRequestedAwaitable, InnerAwaitableType>;
	using ReturnType = typename Super::ReturnType;

	template <typename AwaitableType>
	TDestroyIfAbortRequestedAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
	}

	TDestroyIfAbortRequestedAwaitable(TDestroyIfAbortRequestedAwaitable&&) = default;

	~TDestroyIfAbortRequestedAwaitable()
	{
		if (bSuspended)
		{
			Super::InnerAwaitable.await_abort();
		}
	}

	template <typename HandleType>
	bool await_suspend(HandleType&& AbortablePromiseHandle)
	{
		if (AbortablePromiseHandle.promise().bAbortRequested)
		{
			AbortablePromiseHandle.destroy();
			return true;
		}

		bSuspended = true;
		AbortablePromiseHandle.promise().bSuspendedOnAbortAwaitable = true;

		return Super::await_suspend(Forward<HandleType>(AbortablePromiseHandle));
	}

	bool ShouldResume(const auto& AbortablePromiseHandle, const auto&)
	{
		bSuspended = false;
		AbortablePromiseHandle.promise().bSuspendedOnAbortAwaitable = false;

		return !AbortablePromiseHandle.promise().bAbortRequested;
	}

private:
	bool bSuspended = false;
};

template <typename AwaitableType>
TDestroyIfAbortRequestedAwaitable(AwaitableType&&) -> TDestroyIfAbortRequestedAwaitable<AwaitableType>;


struct FDestroyIfAbortRequestedAdaptor : TAwaitableAdaptorBase<FDestroyIfAbortRequestedAdaptor>
{
	template <CAwaitable AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, FDestroyIfAbortRequestedAdaptor)
	{
		return TDestroyIfAbortRequestedAwaitable{Forward<AwaitableType>(Awaitable)};
	}
};


namespace Awaitables
{
	/**
	 * TAbortablePromise를 상속하는 promise_type은 await_transform에서 Awaitable에 대해 이 함수를 호출해야 합니다.
	 */
	inline FDestroyIfAbortRequestedAdaptor DestroyIfAbortRequested()
	{
		return {};
	}
}
