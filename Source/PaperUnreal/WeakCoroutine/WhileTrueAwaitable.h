// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FilterAwaitable.h"
#include "MinimalAbortableCoroutine.h"
#include "NoDestroyAwaitable.h"


template <typename InnerAwaitableType, typename CoroutineGetterType>
class TWhileTrueAwaitable
{
public:
	using ResultType = decltype(std::declval<InnerAwaitableType>().await_resume());
	
	template <typename AwaitableType, typename CoGetter>
	TWhileTrueAwaitable(AwaitableType&& Awaitable, CoGetter&& Getter)
		: Inner(Forward<AwaitableType>(Awaitable)), CoroutineGetter(Forward<CoGetter>(Getter))
	{
	}

	bool await_ready() const
	{
		return false;
	}

	auto await_suspend(const auto& Handle)
	{
		bAborted = MakeShared<bool>(false);
		Worker = Start();
		Worker->Then([this, Handle, bAborted = bAborted](const auto& FailableResult)
		{
			if (!*bAborted)
			{
				FailableResult ? Handle.resume() : Handle.destroy();
			}
		});
	}

	ResultType await_resume()
	{
		Worker.Reset();
		return MoveTemp(*InnerReturn);
	}
	
	void await_abort()
	{
		*bAborted = true;
		Worker.Reset();
	}

private:
	InnerAwaitableType Inner;
	CoroutineGetterType CoroutineGetter;

	// TODO 로직이 FilterAwaitable과 매우 유사하므로 리팩토링 가능한지 연구
	TAbortableCoroutineHandle<FMinimalAbortableCoroutine> Worker;
	TOptional<ResultType> InnerReturn;
	TSharedPtr<bool> bAborted;

	FMinimalAbortableCoroutine Start()
	{
		while (true)
		{
			InnerReturn = co_await (Inner | Awaitables::If(true));
			if (IsError(*InnerReturn))
			{
				break;
			}

			TAbortableCoroutineHandle Coroutine = CoroutineGetter();
			
			InnerReturn = co_await (Inner | Awaitables::If(false));
			if (IsError(*InnerReturn))
			{
				break;
			}
		}
	}

	template <typename T>
	bool IsError(const TFailableResult<T>& FR)
	{
		return !FR;
	}

	bool IsError(const auto&)
	{
		return false;
	}
};

template <typename AwaitableType, typename CoroutineGetterType>
TWhileTrueAwaitable(AwaitableType&&, CoroutineGetterType&&) -> TWhileTrueAwaitable<AwaitableType, std::decay_t<CoroutineGetterType>>;


template <typename CoroutineGetterType>
struct TWhileTrueAdaptor : TAwaitableAdaptorBase<TWhileTrueAdaptor<CoroutineGetterType>>
{
	std::decay_t<CoroutineGetterType> CoroutineGetter;

	TWhileTrueAdaptor(const std::decay_t<CoroutineGetterType>& Getter)
		: CoroutineGetter(Getter)
	{
	}
	
	TWhileTrueAdaptor(std::decay_t<CoroutineGetterType>&& Getter)
		: CoroutineGetter(MoveTemp(Getter))
	{
	}
	
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TWhileTrueAdaptor Adaptor)
	{
		return TWhileTrueAwaitable{Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.CoroutineGetter)};
	}
};

namespace Awaitables
{
	template <typename CoroutineGetterType>
	auto WhileTrue(CoroutineGetterType&& CoroutineGetter)
	{
		return TWhileTrueAdaptor<CoroutineGetterType>{Forward<CoroutineGetterType>(CoroutineGetter)};
	}
}