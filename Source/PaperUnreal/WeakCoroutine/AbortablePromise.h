// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "ConditionalResumeAwaitable.h"


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

		if (bSuspendedOnAbortIfRequestedAwaitable)
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
	friend class TAbortIfRequestedAwaitable;

	bool bAbortRequested = false;
	bool bSuspendedOnAbortIfRequestedAwaitable = false;

	TWeakPtr<std::monostate> GetPromiseLifeFromDerived() const
	{
		return static_cast<const Derived*>(this)->PromiseLife;
	}
};


template <typename Derived>
class TAbortableCoroutine
{
public:
	~TAbortableCoroutine()
	{
		if (bAbortOnDestruction)
		{
			Abort();
		}
	}

	void Abort()
	{
		if (GetPromiseLife().IsValid())
		{
			GetHandle().promise().Abort();
		}
	}

	void AbortOnDestruction()
	{
		bAbortOnDestruction = true;
	}

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

	friend void operator<<(TArray<TAbortableCoroutine>& Array, TAbortableCoroutine&& Right)
	{
		Array.Add(MoveTemp(Right));
	}

private:
	bool bAbortOnDestruction = false;

	TWeakPtr<std::monostate> GetPromiseLife() const
	{
		return static_cast<const Derived*>(this)->PromiseLife;
	}

	auto GetHandle() const
	{
		return static_cast<const Derived*>(this)->Handle;
	}
};


template <typename AbortableCoroutineType>
class TAbortableCoroutineHandle
{
public:
	TAbortableCoroutineHandle() = default;
	~TAbortableCoroutineHandle() { Reset(); }
	TAbortableCoroutineHandle(TAbortableCoroutineHandle&&) = default;
	TAbortableCoroutineHandle& operator=(TAbortableCoroutineHandle&&) = default;

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

	TAbortIfRequestedAwaitable(TAbortIfRequestedAwaitable&&) = default;

	~TAbortIfRequestedAwaitable()
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
		AbortablePromiseHandle.promise().bSuspendedOnAbortIfRequestedAwaitable = true;

		return Super::await_suspend(Forward<HandleType>(AbortablePromiseHandle));
	}

	bool ShouldResume(const auto& AbortablePromiseHandle, const auto&)
	{
		bSuspended = false;
		AbortablePromiseHandle.promise().bSuspendedOnAbortIfRequestedAwaitable = false;

		return !AbortablePromiseHandle.promise().bAbortRequested;
	}

private:
	bool bSuspended = false;
};

template <typename AwaitableType>
TAbortIfRequestedAwaitable(AwaitableType&&) -> TAbortIfRequestedAwaitable<AwaitableType>;


struct FAbortIfRequestedAdaptor : TAwaitableAdaptorBase<FAbortIfRequestedAdaptor>
{
	template <CAwaitable AwaitableType>
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
