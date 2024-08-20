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
			Awaitable.Reset();
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
	/**
	 * Awaitable Producer를 Wrapping하여 co_await 될 때마다
	 * Awaitable Producer에 co_await을 해 Awaitable을 획득, 해당 Awaitable에 co_await을 하는 Awaitable을 반환합니다.
	 *
	 * 예를 들어서 FSimpleMulticastDelegate는 Awaitable이 아닌 Awaitable Producer이므로 Awaitable 파이프에 연결하기 위해서는
	 * Awaitable이 되어야 합니다. 이 때 단순히 operator co_await를 호출해서 반환된 Awaitable을 파이프에 연결하면
	 * 파이프에 대해 co_await을 단 한 번 밖에 호출할 수 없습니다.
	 * (Delegate가 반환하는 Awaitable은 일회성으로 한 번 co_await하면 소모되어 다시 사용할 수 없음)
	 * 
	 * 그러므로 이 함수를 사용해 Wrapping하면 Delegate 그 자체를 저장하기 때문에 파이프에 연결해도 계속해서 co_await 할 수 있습니다.
	 */
	inline FAwaitableGetterAdaptor TakeAwaitable()
	{
		return {};
	}

	/**
	 * @see TakeAwaitable
	 */
	template <typename MaybeAwaitableType>
	decltype(auto) TakeAwaitableOrForward(MaybeAwaitableType&& MaybeAwaitable)
	{
		if constexpr (CAwaitable<MaybeAwaitableType>)
		{
			return Forward<MaybeAwaitableType>(MaybeAwaitable);
		}
		else
		{
			return Forward<MaybeAwaitableType>(MaybeAwaitable) | TakeAwaitable();
		}
	}
}


/**
 * Awaitable이 아닌 타입에 대해 pipe operator를 호출하면 해당 타입이 Awaitable Producer라고 가정하고
 * AwaitableGetterAwaitable을 씌운 후 Derived Class의 pipe operator에 넘깁니다.
 *
 * Awaitable은 await_ 함수들을 구현한 클래스이며
 * Awaitable Producer는 operator co_await을 구현해 Awaitable을 반환할 수 있는 클래스를 말합니다.
 */
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
