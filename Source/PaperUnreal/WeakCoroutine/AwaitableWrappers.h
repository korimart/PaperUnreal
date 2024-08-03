// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalCoroutine.h"
#include "TypeTraits.h"


template <typename ValueStreamType, typename PredicateType>
class TFilteringAwaitable
{
public:
	using ResultType = typename std::decay_t<ValueStreamType>::ResultType;

	// TODO stream일 필요가 없음 여러번 await 가능한 awaitable이면 됨
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

	void await_abort()
	{
		// TODO await cancel all child coroutines
	}

private:
	ValueStreamType ValueStream;
	PredicateType Predicate;
	TOptional<TFailableResult<ResultType>> Ret;
};

template <typename Stream, typename Pred>
TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
// TODO 단순화 가능
	-> TFilteringAwaitable<std::conditional_t<std::is_lvalue_reference_v<Stream>, Stream, std::decay_t<Stream>>, std::decay_t<Pred>>;


template <typename AwaitableType, typename TransformFuncType>
class TTransformingAwaitable
{
public:
	template <typename Trans>
	TTransformingAwaitable(AwaitableType&& Inner, Trans&& InTransformFunc)
		: Awaitable(MoveTemp(Inner)), TransformFunc(Forward<Trans>(InTransformFunc))
	{
	}

	bool await_ready() const
	{
		return Awaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		return Awaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		if constexpr (std::is_void_v<decltype(Awaitable.await_resume())>)
		{
			Awaitable.await_resume();
			return TransformFunc();
		}
		else
		{
			return TransformFunc(Awaitable.await_resume());
		}
	}

	void await_abort()
	{
		Awaitable.await_abort();
	}

private:
	AwaitableType Awaitable;
	TransformFuncType TransformFunc;
};

template <typename Await, typename Trans>
TTransformingAwaitable(Await&& Awaitable, Trans&& TransformFunc)
	-> TTransformingAwaitable<std::decay_t<Await>, std::decay_t<Trans>>;


template <typename ValueStreamType, typename PredicateType>
auto Filter(ValueStreamType&& Stream, PredicateType&& Predicate)
{
	return TFilteringAwaitable{Forward<ValueStreamType>(Stream), Forward<PredicateType>(Predicate)};
}


template <typename MaybeAwaitableType, typename TransformFuncType>
auto Transform(MaybeAwaitableType&& MaybeAwaitable, TransformFuncType&& TransformFunc)
{
	return TTransformingAwaitable
	{
		AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable)),
		Forward<TransformFuncType>(TransformFunc),
	};
}
