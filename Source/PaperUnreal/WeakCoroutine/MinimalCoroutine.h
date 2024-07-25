// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>
#include "CoreMinimal.h"


struct FMinimalCoroutine
{
	struct promise_type
	{
		FMinimalCoroutine get_return_object()
		{
			return {};
		}

		std::suspend_never initial_suspend() const
		{
			return {};
		}

		std::suspend_never final_suspend() const noexcept
		{
			return {};
		}

		void return_void() const
		{
		}

		void unhandled_exception() const
		{
		}
	};
};


template <typename AwaitableType>
class TIdentityAwaitable
{
public:
	TIdentityAwaitable(AwaitableType&& InAwaitable)
		: Awaitable(MoveTemp(InAwaitable))
	{
	}

	TIdentityAwaitable(const TIdentityAwaitable&) = delete;
	TIdentityAwaitable& operator=(const TIdentityAwaitable&) = delete;

	TIdentityAwaitable(TIdentityAwaitable&&) = default;
	TIdentityAwaitable& operator=(TIdentityAwaitable&&) = delete;

	bool await_ready() const
	{
		return Awaitable.await_ready();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		Awaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		return Awaitable.await_resume();
	}

	AwaitableType& Inner()
	{
		return Awaitable;
	}

private:
	AwaitableType Awaitable;
};