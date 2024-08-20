// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "MinimalAbortableCoroutine.h"
#include "NoDestroyAwaitable.h"


template <typename... InnerAwaitableTypes>
class TAnyOfAwaitable
{
public:
	template <typename... AwaitableTypes>
	TAnyOfAwaitable(AwaitableTypes&&... Awaitables)
		: InnerAwaitables(Forward<AwaitableTypes>(Awaitables)...)
	{
		static_assert(CErrorReportingAwaitable<TAnyOfAwaitable>);
	}

	TAnyOfAwaitable(TAnyOfAwaitable&&) = default;

	~TAnyOfAwaitable()
	{
		CancelCoroutines();
	}

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	void await_suspend(const HandleType& Handle)
	{
		bInsideAwaitSuspend = true;

		FinishedCoroutineIndices.Empty();
		SucceededCoroutineIndex.Reset();

		[&]<size_t... Indices>(std::index_sequence<Indices...>)
		{
			(Coroutines.Add(WaitOnInnerAwaitable<Indices>(Handle)), ...);
		}(std::index_sequence_for<InnerAwaitableTypes...>{});

		bInsideAwaitSuspend = false;

		ResumeIfShouldResume(Handle);
	}

	TFailableResult<int32> await_resume()
	{
		CancelCoroutines();

		if (SucceededCoroutineIndex)
		{
			return *SucceededCoroutineIndex;
		}

		return NewError(TEXT("TAnyOfAwaitable 아무도 에러 없이 완료하지 않았음"));
	}

	void await_abort()
	{
		CancelCoroutines();
	}

private:
	TTuple<InnerAwaitableTypes...> InnerAwaitables;
	TArray<FMinimalAbortableCoroutine> Coroutines;
	TSet<int32> FinishedCoroutineIndices;
	TOptional<int32> SucceededCoroutineIndex;
	bool bInsideAwaitSuspend = false;

	void CancelCoroutines()
	{
		for (FMinimalAbortableCoroutine& Each : Coroutines)
		{
			Each.Abort();
		}

		Coroutines.Empty();
	}

	template <int32 Index>
	FMinimalAbortableCoroutine WaitOnInnerAwaitable(auto Handle)
	{
		if (co_await (InnerAwaitables.template Get<Index>() | Awaitables::NoDestroy()))
		{
			if (!SucceededCoroutineIndex)
			{
				SucceededCoroutineIndex = Index;
			}
		}

		FinishedCoroutineIndices.Add(Index);
		ResumeIfShouldResume(Handle);
	}

	void ResumeIfShouldResume(const auto& Handle)
	{
		if (!bInsideAwaitSuspend
			&& (SucceededCoroutineIndex
				|| FinishedCoroutineIndices.Num() >= sizeof...(InnerAwaitableTypes)))
		{
			Handle.resume();
		}
	}
};

template <typename... AwaitableTypes>
TAnyOfAwaitable(AwaitableTypes&&...) -> TAnyOfAwaitable<AwaitableTypes...>;


namespace Awaitables
{
	/**
	 * Awaitable 또는 Awaitable Producer를 파라미터로 받아 각각에 대해 co_await을 호출하고
	 * 그 중 하나라도 완료되는 타이밍에 resume하는 Awaitable을 반환합니다.
	 *
	 * Awaitable은 await_ 함수들을 구현한 클래스이며
	 * Awaitable Producer는 operator co_await을 구현해 Awaitable을 반환할 수 있는 클래스를 말합니다.
	 *
	 * 이 함수에 대한 예시는 AnyOfAwaitableTest.cpp를 참고해주세요
	 */
	template <typename... MaybeAwaitableTypes>
	auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
	{
		return TAnyOfAwaitable{TakeAwaitableOrForward(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...};
	}
}