// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"


template <typename T>
struct TReadyAwaitable
{
	T Value;
	
	bool await_ready() const
	{
		return true;
	}

	void await_suspend(const auto&)
	{
	}

	T await_resume() &
	{
		return Value;
	}
	
	T await_resume() &&
	{
		return MoveTemp(Value);
	}
};


namespace Awaitables
{
	template <typename T>
	TReadyAwaitable<std::decay_t<T>> AsAwaitable(T&& Value)
	{
		return {Forward<T>(Value)};
	}
}


struct FSuspendConditional {
	bool bSuspend = false;
    constexpr bool await_ready() const noexcept { return !bSuspend; }
    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};