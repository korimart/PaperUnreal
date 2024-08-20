// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>
#include "CoreMinimal.h"
#include "AbortablePromise.h"
#include "AwaitablePromise.h"


template <typename T>
struct TMinimalAbortableCoroutine
	: TAwaitableCoroutine<TMinimalAbortableCoroutine<T>, T>
	  , TAbortableCoroutine<TMinimalAbortableCoroutine<T>>
{
	struct promise_type : TAbortablePromise<promise_type>, TAwaitablePromise<T>
	{
		TSharedRef<std::monostate> PromiseLife = MakeShared<std::monostate>();

		TMinimalAbortableCoroutine get_return_object()
		{
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}

		std::suspend_never initial_suspend() const
		{
			return {};
		}

		std::suspend_never final_suspend() const noexcept
		{
			return {};
		}

		void unhandled_exception() const
		{
		}

		template <typename AwaitableType>
		auto await_transform(AwaitableType&& Awaitable)
		{
			return Forward<AwaitableType>(Awaitable) | Awaitables::DestroyIfAbortRequested();
		}

		void OnAbortRequested()
		{
		}

		// TODO 여기다가 쓰는 게 아니라 awaitable 쪽에서 SetErrors가 있는지 검사
		void SetErrors(const TArray<UFailableResultError*>&)
		{
			// 미니멀이기 때문에 DestroyIfError 등의 Awaitable을 사용해도
			// 어떤 에러가 발생했는지 기록하지 않음
		}
	};

	TWeakPtr<std::monostate> PromiseLife;
	std::coroutine_handle<promise_type> Handle;

	TMinimalAbortableCoroutine(std::coroutine_handle<promise_type> InHandle)
		: TAwaitableCoroutine<TMinimalAbortableCoroutine, T>(InHandle)
		  , PromiseLife(InHandle.promise().PromiseLife)
		  , Handle(InHandle)
	{
	}

	TMinimalAbortableCoroutine(TMinimalAbortableCoroutine&&) = default;
	TMinimalAbortableCoroutine& operator=(TMinimalAbortableCoroutine&&) = default;

	void OnAwaitAbort()
	{
		this->Abort();
	}
};


using FMinimalAbortableCoroutine = TMinimalAbortableCoroutine<void>;
