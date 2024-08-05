// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"


template <typename AwaitableProducerType>
class TAwaitableGetterAwaitable
{
public:
	// TODO 여기에 operator co_await 존재 여부에 대한 static assert 추가
	using AwaitableType = decltype(operator co_await(std::declval<AwaitableProducerType&>()));

	TAwaitableGetterAwaitable(AwaitableProducerType Producer)
		requires std::is_lvalue_reference_v<AwaitableProducerType>
		: AwaitableProducer(Producer)
	{
	}

	TAwaitableGetterAwaitable(AwaitableProducerType&& Producer)
		requires !std::is_reference_v<AwaitableProducerType>
		: AwaitableProducer(MoveTemp(Producer))
	{
	}

	bool await_ready() const
	{
		return const_cast<TAwaitableGetterAwaitable*>(this)->await_ready();
	}

	bool await_ready()
	{
		if (!Awaitable)
		{
			Awaitable.Emplace(operator co_await(AwaitableProducer));
		}

		return Awaitable->await_ready();
	}

	auto await_suspend(const auto& Handle)
	{
		return Awaitable->await_suspend(Handle);
	}

	auto await_resume()
	{
		auto Ret = Awaitable->await_resume();
		Awaitable.Reset();
		return Ret;
	}

	void await_abort()
	{
		if (Awaitable)
		{
			Awaitable->await_abort();
		}
	}

private:
	AwaitableProducerType AwaitableProducer;
	TOptional<AwaitableType> Awaitable;
};


struct FAwaitableGetterAdaptor
{
	template <typename NotAwaitableType> requires !CAwaitable<NotAwaitableType>
	friend auto operator|(NotAwaitableType&& NotAwaitable, FAwaitableGetterAdaptor)
	{
		return TAwaitableGetterAwaitable<NotAwaitableType>{Forward<NotAwaitableType>(NotAwaitable)};
	}
};


namespace Awaitables
{
	inline FAwaitableGetterAdaptor TakeAwaitable()
	{
		return {};
	}
}


template <typename DerivedType>
struct TAwaitableAdaptorBase
{
	template <typename NotAwaitableType> requires !CAwaitable<NotAwaitableType>
	friend decltype(auto) operator|(NotAwaitableType&& NotAwaitable, const DerivedType& Derived)
	{
		return Forward<NotAwaitableType>(NotAwaitable)
			| Awaitables::TakeAwaitable()
			| Derived;
	}

	template <typename NotAwaitableType> requires !CAwaitable<NotAwaitableType>
	friend decltype(auto) operator|(NotAwaitableType&& NotAwaitable, DerivedType&& Derived)
	{
		return Forward<NotAwaitableType>(NotAwaitable)
			| Awaitables::TakeAwaitable()
			| MoveTemp(Derived);
	}
};
