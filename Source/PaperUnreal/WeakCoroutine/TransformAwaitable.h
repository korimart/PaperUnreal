// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "TypeTraits.h"


template <bool bIfNotError, typename InnerAwaitableType, typename TransformFuncType>
class TTransformAwaitable
{
public:
	template <typename AwaitableType, typename Trans>
	TTransformAwaitable(AwaitableType&& Awaitable, Trans&& InTransformFunc)
		: InnerAwaitable(MoveTemp(Awaitable)), TransformFunc(Forward<Trans>(InTransformFunc))
	{
	}

	bool await_ready() const
	{
		return InnerAwaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		return InnerAwaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume() requires bIfNotError
	{
		auto FailableResult = InnerAwaitable.await_resume();
		using ResultType = typename decltype(FailableResult)::ResultType;

		if constexpr (std::is_void_v<ResultType>)
		{
			using TransformedType = decltype(TransformFunc());
			using ReturnType = TFailableResult<TransformedType>;
			return FailableResult ? ReturnType{TransformFunc()} : ReturnType{FailableResult.GetErrors()};
		}
		else if constexpr (TIsInstantiationOf_V<ResultType, TTuple>)
		{
			return [&]<typename... TupleElementTypes>(TTypeList<TupleElementTypes...>)
			{
				using TransformedType = decltype(TransformFunc(std::declval<TupleElementTypes>()...));
				using ReturnType = TFailableResult<TransformedType>;

				if (!FailableResult)
				{
					return ReturnType{FailableResult.GetErrors()};
				}

				return FailableResult.GetResult().ApplyAfter([&]<typename... ArgTypes>(ArgTypes&&... Args)
				{
					return ReturnType{TransformFunc(Forward<ArgTypes>(Args)...)};
				});
			}(TSwapTypeList<ResultType, TTypeList>{});
		}
		else
		{
			using TransformedType = decltype(TransformFunc(FailableResult.GetResult()));
			using ReturnType = TFailableResult<TransformedType>;
			return FailableResult ? ReturnType{TransformFunc(FailableResult.GetResult())} : ReturnType{FailableResult.GetErrors()};
		}
	}

	auto await_resume() requires !bIfNotError
	{
		if constexpr (std::is_void_v<decltype(InnerAwaitable.await_resume())>)
		{
			InnerAwaitable.await_resume();
			return TransformFunc();
		}
		else if constexpr (TIsInstantiationOf_V<decltype(InnerAwaitable.await_resume()), TTuple>)
		{
			return InnerAwaitable.await_resume().ApplyAfter([&]<typename... ArgTypes>(ArgTypes&&... Args)
			{
				return TransformFunc(Forward<ArgTypes>(Args)...);
			});
		}
		else
		{
			return TransformFunc(InnerAwaitable.await_resume());
		}
	}

	void await_abort()
	{
		InnerAwaitable.await_abort();
	}

private:
	InnerAwaitableType InnerAwaitable;
	TransformFuncType TransformFunc;
};


template <bool bIfNotError, typename TransformFuncType>
struct TTransformAdaptor : TAwaitableAdaptorBase<TTransformAdaptor<bIfNotError, TransformFuncType>>
{
	std::decay_t<TransformFuncType> Predicate;

	TTransformAdaptor(const std::decay_t<TransformFuncType>& Pred)
		: Predicate(Pred)
	{
	}

	TTransformAdaptor(std::decay_t<TransformFuncType>&& Pred)
		: Predicate(MoveTemp(Pred))
	{
	}

	template <CErrorReportingAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TTransformAdaptor Adaptor) requires bIfNotError
	{
		return TTransformAwaitable<true, AwaitableType, decltype(Predicate)>
		{
			Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.Predicate)
		};
	}

	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TTransformAdaptor Adaptor) requires !bIfNotError
	{
		return TTransformAwaitable<false, AwaitableType, decltype(Predicate)>
		{
			Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.Predicate)
		};
	}
};


namespace Awaitables
{
	template <typename TransformFuncType>
	auto Transform(TransformFuncType&& Predicate)
	{
		return TTransformAdaptor<false, TransformFuncType>{Forward<TransformFuncType>(Predicate)};
	}
	
	template <typename TransformFuncType>
	auto TransformIfNotError(TransformFuncType&& Predicate)
	{
		return TTransformAdaptor<true, TransformFuncType>{Forward<TransformFuncType>(Predicate)};
	}
}
