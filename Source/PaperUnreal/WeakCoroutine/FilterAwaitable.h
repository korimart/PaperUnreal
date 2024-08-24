// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalAbortableCoroutine.h"


/**
 * @see Awaitables::FilterWithError
 */
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
		Coroutine->Then([Handle, bAborted = bAborted](const auto& FailableResult)
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


namespace FilterDetails
{
	template <typename PredicateType>
	auto FilterImpl(PredicateType&& Predicate)
	{
		auto Relay = [Predicate = Forward<PredicateType>(Predicate)]
			<typename FailableResultType>(const FailableResultType& FailableResult) mutable
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
}


namespace Awaitables
{
	/**
	 * Awaitable을 받아서 co_await를 호출한 결과에 대해
	 * Predicate가 true를 반환한 것을 반환하는 Awaitable을 반환합니다.
	 * false를 반환하면 true를 반환할 때까지 Awaitable에 co_await를 다시 호출합니다.
	 *
	 * 왜 WithError가 붙어있는지는 Filter 함수 참고
	 */
	template <typename PredicateType>
	auto FilterWithError(PredicateType&& Predicate)
	{
		return TFilterAdaptor<PredicateType>{Forward<PredicateType>(Predicate)};
	}

	/**
	 * Awaitable을 받아서 co_await를 호출한 결과에 대해
	 * Predicate가 true를 반환한 것을 반환하는 Awaitable을 반환합니다.
	 * false를 반환하면 true를 반환할 때까지 Awaitable에 co_await를 다시 호출합니다.
	 *
	 * 만약 Awaitable에 co_await을 호출한 결과가 TFailableResult이면
	 * Succeeded 상태인 경우에만 Predicate를 GetResult로 호출합니다.
	 * Failed 상태이면 Filter하지 않고 그대로 반환합니다. (즉 에러가 Propagate 됨)
	 */
	template <typename PredicateType>
	auto Filter(PredicateType&& Predicate)
	{
		return FilterDetails::FilterImpl(Forward<PredicateType>(Predicate));
	}

	/**
	 * ValueType의 변수를 선언해 가장 최근의 결과를 저장하고 중복된 결과를 걸러내 값이 달라질 경우에만
	 * 코루틴을 resume하는 Awaitable을 반환합니다.
	 *
	 * IntAwaitable이 int32를 반환하는 Awaitable이면,
	 * 
	 * const int32 Value0 = co_await IntAwaitable;
	 * const int32 Value1 = co_await IntAwaitable;
	 * const int32 Value2 = co_await IntAwaitable;
	 * 의 값이 모두 동일할 수 있습니다. (IntAwaitable이 같은 값을 세 번 반환한 경우)
	 *
	 * auto NoDuplicate = IntAwaitable | Awaitables::FilterDuplicate<int32>();
	 * const int32 Value0 = co_await NoDuplicate;
	 * const int32 Value1 = co_await NoDuplicate;
	 * const int32 Value2 = co_await NoDuplicate;
	 * 이 경우 값의 변화 없이는 NoDuplicate이 co_await을 완료하지 않습니다.
	 */
	template <typename ValueType>
	auto FilterDuplicate()
	{
		return Filter([LastValue = TOptional<ValueType>{}](const ValueType& Value) mutable
		{
			const bool bLetItPass = !LastValue || *LastValue != Value;
			LastValue = Value;
			return bLetItPass;
		});
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

	template <typename ValueType>
	auto IfLessThan(ValueType&& Value)
	{
		return Filter([Value = Forward<ValueType>(Value)](const auto& Result) { return Result < Value; });
	}

	inline auto IfValid()
	{
		return FilterWithError([](const auto& Result)
		{
			if (Result)
			{
				return ::IsValid(Result.GetResult());
			}

			return !Result.template OnlyContains<UInvalidObjectError>();
		});
	}

	inline auto IfNotNull()
	{
		return Filter([](const auto& Result) { return !Result.IsNull(); });
	}

	/**
	 * Awaitable을 받아서 co_await를 호출한 결과가 TFailableResult라고 가정하고
	 * 발생한 에러가 ErrorTypes의 부분집합이면 이 결과를 버리는 Awaitable을 반환합니다.
	 */
	template <typename... ErrorTypes>
	auto IgnoreError()
	{
		return FilterWithError([](const auto& Result)
		{
			if (Result)
			{
				return true;
			}

			return !Result.template OnlyContains<ErrorTypes...>();
		});
	}
}
