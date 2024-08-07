// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalAbortableCoroutine.h"


template <typename InnerAwaitableType, typename PredicateType>
class TFilterAwaitable
{
public:
	using ResultType = decltype(std::declval<InnerAwaitableType>().await_resume());

	template <typename AwaitableType, typename Pred>
	TFilterAwaitable(AwaitableType&& Awaitable, Pred&& Predicate)
		: InnerAwaitable(Forward<AwaitableType>(Awaitable)), Predicate(Forward<Pred>(Predicate))
	{
	}

	bool await_ready() const
	{
		return false;
	}

	void await_suspend(const auto& Handle)
	{
		bAborted = MakeShared<bool>(false);
		Coroutine = StartFiltering();
		Coroutine->ReturnValue().Then([Handle, bAborted = bAborted](const auto& FailableResult)
		{
			if (!*bAborted)
			{
				FailableResult ? Handle.resume() : Handle.destroy();
			}
		});
	}

	ResultType await_resume()
	{
		Coroutine.Reset();
		return MoveTemp(Ret.GetValue());
	}

	void await_abort()
	{
		*bAborted = true;
		Coroutine.Reset();
	}

private:
	InnerAwaitableType InnerAwaitable;
	PredicateType Predicate;
	TAbortableCoroutineHandle<FMinimalAbortableCoroutine> Coroutine;
	TOptional<ResultType> Ret;
	TSharedPtr<bool> bAborted;

	FMinimalAbortableCoroutine StartFiltering()
	{
		while (true)
		{
			Ret = co_await InnerAwaitable;
			if (Predicate(*Ret))
			{
				break;
			}
		}
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
	auto FilterWithError(PredicateType&& Predicate)
	{
		return TFilterAdaptor<PredicateType>{Forward<PredicateType>(Predicate)};
	}

	template <typename PredicateType>
	auto Filter(PredicateType&& Predicate)
	{
		auto Relay = [Predicate = Forward<PredicateType>(Predicate)]
			<typename FailableResultType>(const FailableResultType& FailableResult)
		{
			// FilterWithError를 사용할 자리에 Filter를 사용하면 여기 걸림
			static_assert(TIsInstantiationOf_V<FailableResultType, TFailableResult>);

			if (!FailableResult)
			{
				// 여기서 true의 의미는 FilterAwaitbale이 현재 값을 다음 Awaitable에 반환할지 여부임
				// 에러 발생 시 에러를 그대로 던지고 싶으므로 true를 반환하는 것이 맞음
				return true;
			}

			return Predicate(FailableResult.GetResult());
		};

		return TFilterAdaptor<decltype(Relay)>{MoveTemp(Relay)};
	}
	
	template <typename ValueType>
	auto If(ValueType&& Value)
	{
		return Filter([Value = Forward<ValueType>(Value)](const auto& Result) { return Result == Value; });
	}
	
	template <typename ValueType>
	auto IfNot(ValueType&& Value)
	{
		return Filter([Value = Forward<ValueType>(Value)](const auto& Result) { return Result != Value; });
	}
	
	inline auto IfValid()
	{
		return Filter([](auto* Result) { return ::IsValid(Result); });
	}

}