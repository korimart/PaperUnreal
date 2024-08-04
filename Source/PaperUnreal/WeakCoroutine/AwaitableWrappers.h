// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"


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


template <typename MaybeAwaitableType, typename TransformFuncType>
auto Transform(MaybeAwaitableType&& MaybeAwaitable, TransformFuncType&& TransformFunc)
{
	return TTransformingAwaitable
	{
		AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable)),
		Forward<TransformFuncType>(TransformFunc),
	};
}
