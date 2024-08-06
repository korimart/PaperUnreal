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

		// TODO 여기다가 쓰는 게 아니라 awaitable 쪽에서 SetErrors가 있는지 검사
		void SetErrors(const TArray<UFailableResultError*>&)
		{
			// 미니멀이기 때문에 AbortIfError 등의 Awaitable을 사용해도
			// 어떤 에러가 발생했는지 기록하지 않음
		}
	};

	TWeakPtr<std::monostate> PromiseLife;
	std::coroutine_handle<promise_type> CoroutineHandle;
	bool bAbortOnDestruction = false;

	~FMinimalAbortableCoroutine()
	{
		if (bAbortOnDestruction)
		{
			Abort();
		}
	}

	void Abort()
	{
		if (PromiseLife.IsValid())
		{
			CoroutineHandle.promise().Abort();
		}
	}

	void AbortOnDestruction()
	{
		bAbortOnDestruction = true;
	}
};
