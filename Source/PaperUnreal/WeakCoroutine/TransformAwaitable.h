// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "TypeTraits.h"


template <typename InnerAwaitableType, typename TransformFuncType>
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

	auto await_resume()
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

template <typename AwaitableType, typename Trans>
TTransformAwaitable(AwaitableType&& Awaitable, Trans&& TransformFunc) -> TTransformAwaitable<AwaitableType, std::decay_t<Trans>>;


template <typename TransformFuncType>
struct TTransformAdaptor : TAwaitableAdaptorBase<TTransformAdaptor<TransformFuncType>>
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

	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TTransformAdaptor Adaptor)
	{
		return TTransformAwaitable{Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.Predicate)};
	}
};


namespace Awaitables
{
	template <typename TransformFuncType>
	auto Transform(TransformFuncType&& Predicate)
	{
		return TTransformAdaptor<TransformFuncType>{Forward<TransformFuncType>(Predicate)};
	}
}
