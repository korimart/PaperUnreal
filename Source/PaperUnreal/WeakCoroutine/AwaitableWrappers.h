// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "MinimalCoroutine.h"
#include "TypeTraits.h"


template <typename... InAwaitableTypes>
class TAnyOfAwaitable
{
public:
	using AwaitableTuple = TTuple<typename TEnsureErrorReporting<InAwaitableTypes>::Type...>;
	using ResultType = int32;

	TAnyOfAwaitable(InAwaitableTypes&&... InAwaitables)
		: Awaitables(MoveTemp(InAwaitables)...)
	{
		static_assert(CErrorReportingAwaitable<TAnyOfAwaitable>);
	}

	TAnyOfAwaitable(const TAnyOfAwaitable&) = delete;
	TAnyOfAwaitable& operator=(const TAnyOfAwaitable&) = delete;

	TAnyOfAwaitable(TAnyOfAwaitable&&) = default;
	TAnyOfAwaitable& operator=(TAnyOfAwaitable&&) = default;

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	void await_suspend(const HandleType& Handle)
	{
		TSharedRef<int32> Counter = MakeShared<int32>(sizeof...(InAwaitableTypes));
		ForEachTupleElementIndexed(Awaitables, [&](int32 Index, auto& Awaitable)
		{
			[](auto Awaitable, int32 Index, HandleType Handle,
			   TSharedRef<int32> ResultIndex, TSharedRef<int32> CleanUpCounter) -> FMinimalCoroutine
			{
				auto Result = co_await Awaitable;

				--(*CleanUpCounter);

				if (*ResultIndex < 0 && Result.Succeeded())
				{
					*ResultIndex = Index;
					Handle.resume();
					co_return;
				}

				if (*ResultIndex < 0 && *CleanUpCounter <= 0)
				{
					Handle.resume();
				}
			}(MoveTemp(Awaitable), Index, Handle, ResultIndex, Counter);
		});
	}

	TFailableResult<int32> await_resume()
	{
		if (*ResultIndex >= 0)
		{
			return *ResultIndex;
		}

		return NewError(TEXT("TAnyOfAwaitable 아무도 에러 없이 완료하지 않았음"));
	}

private:
	AwaitableTuple Awaitables;
	TSharedRef<int32> ResultIndex = MakeShared<int32>(-1);
};


template <typename ValueStreamType, typename PredicateType>
class TFilteringAwaitable
{
public:
	using ResultType = typename std::decay_t<ValueStreamType>::ResultType;

	template <typename Stream, typename Pred>
	TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
		: ValueStream(Forward<Stream>(ValueStream)), Predicate(Forward<Pred>(Predicate))
	{
		static_assert(CErrorReportingAwaitable<TFilteringAwaitable>);
	}

	bool await_ready() const
	{
		return false;
	}

	void await_suspend(auto Handle)
	{
		[](TFilteringAwaitable& This, auto Handle) -> FMinimalCoroutine
		{
			while (true)
			{
				auto Result = co_await This.ValueStream;
				if (Result.Failed() || This.Predicate(Result.GetResult()))
				{
					This.Ret = MoveTemp(Result);
					break;
				}
			}

			Handle.resume();
		}(*this, Handle);
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(Ret.GetValue());
	}

private:
	ValueStreamType ValueStream;
	PredicateType Predicate;
	TOptional<TFailableResult<ResultType>> Ret;
};

template <typename Stream, typename Pred>
TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
	-> TFilteringAwaitable<std::conditional_t<std::is_lvalue_reference_v<Stream>, Stream, std::decay_t<Stream>>, std::decay_t<Pred>>;


template <typename... MaybeAwaitableTypes>
auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
{
	return TAnyOfAwaitable{AwaitableOrIdentity(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...};
}


template <typename ValueStreamType, typename PredicateType>
auto Filter(ValueStreamType&& Stream, PredicateType&& Predicate)
{
	return TFilteringAwaitable{Forward<ValueStreamType>(Stream), Forward<PredicateType>(Predicate)};
}
