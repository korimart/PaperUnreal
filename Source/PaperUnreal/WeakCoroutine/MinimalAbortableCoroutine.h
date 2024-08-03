// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>
#include "CoreMinimal.h"
#include "AbortablePromise.h"


struct FMinimalAbortableCoroutine
{
	struct promise_type : TAbortablePromise<promise_type>
	{
		TSharedRef<std::monostate> PromiseLife = MakeShared<std::monostate>();

		FMinimalAbortableCoroutine get_return_object()
		{
			return {PromiseLife, std::coroutine_handle<promise_type>::from_promise(*this)};
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

		template <typename AwaitableType>
		auto await_transform(AwaitableType&& Awaitable)
		{
			return Forward<AwaitableType>(Awaitable) | Awaitables::AbortIfRequested();
		}

		void OnAbortRequested()
		{
		}
	};

	TWeakPtr<std::monostate> PromiseLife;
	std::coroutine_handle<promise_type> CoroutineHandle;

	void Abort()
	{
		if (PromiseLife.IsValid())
		{
			CoroutineHandle.promise().Abort();
		}
	}
};
