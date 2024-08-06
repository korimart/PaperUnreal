// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalAbortableCoroutine.h"
#include "NoDestroyAwaitable.h"


template <typename InnerAwaitableType, typename PredicateType>
class TFilterAwaitable
{
public:
	using ResultType = typename decltype(std::declval<InnerAwaitableType>() | Awaitables::NoDestroy())::ResultType;

	template <typename AwaitableType, typename Pred>
	TFilterAwaitable(AwaitableType&& Awaitable, Pred&& Predicate)
		: InnerAwaitable(Forward<AwaitableType>(Awaitable)), Predicate(Forward<Pred>(Predicate))
	{
		static_assert(CErrorReportingAwaitable<TFilterAwaitable>);
	}

	bool await_ready() const
	{
		return false;
	}

	void await_suspend(const auto& Handle)
	{
		Coroutine = StartFiltering(Handle);
	}

	// TODO Inner가 Error reporing이면 에러로 아니면 아닌걸로 변경
	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(Ret.GetValue());
	}

	void await_abort()
	{
		if (Coroutine)
		{
			Coroutine->Abort();
		}
	}

private:
	InnerAwaitableType InnerAwaitable;
	PredicateType Predicate;
	TOptional<FMinimalAbortableCoroutine> Coroutine;
	TOptional<TFailableResult<ResultType>> Ret;

	FMinimalAbortableCoroutine StartFiltering(auto Handle)
	{
		while (true)
		{
			auto Result = co_await (InnerAwaitable | Awaitables::NoDestroy());
			if (Result.Failed() || Predicate(Result.GetResult()))
			{
				Ret = MoveTemp(Result);
				break;
			}
		}

		Handle.resume();
	}
};

template <typename AwaitableType, typename Pred>
TFilterAwaitable(AwaitableType&&, Pred&&) -> TFilterAwaitable<AwaitableType, std::decay_t<Pred>>;


template <typename PredicateType>
struct TFilterAdaptor : TAwaitableAdaptorBase<TFilterAdaptor<PredicateType>>
{
	std::decay_t<PredicateType> Predicate;

	TFilterAdaptor(const std::decay_t<PredicateType>& Pred)
		: Predicate(Pred)
	{
	}
	
	TFilterAdaptor(std::decay_t<PredicateType>&& Pred)
		: Predicate(MoveTemp(Pred))
	{
	}
	
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TFilterAdaptor Adaptor)
	{
		return TFilterAwaitable{Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.Predicate)};
	}
};

namespace Awaitables
{
	template <typename PredicateType>
	auto Filter(PredicateType&& Predicate)
	{
		return TFilterAdaptor<PredicateType>{Forward<PredicateType>(Predicate)};
	}

	template <typename ValueType>
	auto If(ValueType&& Value)
	{
		return Filter([Value = Forward<ValueType>(Value)](const auto& Result) { return Result == Value; });
	}
}