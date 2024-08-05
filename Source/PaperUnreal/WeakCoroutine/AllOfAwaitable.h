// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "FilterAwaitable.h"
#include "MinimalAbortableCoroutine.h"


template <typename... InnerAwaitableTypes>
class TAllOfAwaitable
{
public:
	template <typename... AwaitableTypes>
	TAllOfAwaitable(AwaitableTypes&&... Awaitables)
		: InnerAwaitables(Forward<AwaitableTypes>(Awaitables)...)
	{
		static_assert(CErrorReportingAwaitable<TAllOfAwaitable>);
	}

	TAllOfAwaitable(TAllOfAwaitable&&) = default;

	~TAllOfAwaitable()
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

		ChildCoroutineErrors.Empty();
		ChildCoroutines.Empty();
		TrueAwaitables.Empty();

		[&]<size_t... Indices>(std::index_sequence<Indices...>)
		{
			(ChildCoroutines.Add(InitiateAwaitableTrueCheck<Indices>(Handle)), ...);
		}(std::index_sequence_for<InnerAwaitableTypes...>{});

		bInsideAwaitSuspend = false;

		ResumeIfShouldResume(Handle);
	}

	// TODO TFailalbeResult가 void를 받게 되면 void로 수정
	TFailableResult<std::monostate> await_resume()
	{
		CancelCoroutines();

		if (ChildCoroutineErrors.Num() > 0)
		{
			TFailableResult<std::monostate> Ret{ChildCoroutineErrors};
			Ret.AddError(NewError(TEXT("AllOfAwaitable: awaitable 중의 하나가 에러를 반환했음")));
			return Ret;
		}

		return std::monostate{};
	}

	void await_abort()
	{
		CancelCoroutines();
	}

private:
	TTuple<InnerAwaitableTypes...> InnerAwaitables;
	TArray<FMinimalAbortableCoroutine> ChildCoroutines;
	TSet<int32> TrueAwaitables;
	TArray<UFailableResultError*> ChildCoroutineErrors;
	bool bInsideAwaitSuspend = false;

	void CancelCoroutines()
	{
		for (FMinimalAbortableCoroutine& Each : ChildCoroutines)
		{
			Each.Abort();
		}

		ChildCoroutines.Empty();
	}

	template <int32 Index>
	FMinimalAbortableCoroutine InitiateAwaitableTrueCheck(auto Handle)
	{
		auto Awaitable = InnerAwaitables.template Get<Index>() | Awaitables::NoDestroy();

		while (true)
		{
			TFailableResult<bool> Result = co_await Awaitable;

			if (!Result)
			{
				ChildCoroutineErrors = Result.GetErrors();
				ResumeIfShouldResume(Handle);
				co_return;
			}

			Result.GetResult() ? (void)TrueAwaitables.Add(Index) : (void)TrueAwaitables.Remove(Index);
			ResumeIfShouldResume(Handle);
		}
	}

	void ResumeIfShouldResume(const auto& Handle)
	{
		if (!bInsideAwaitSuspend
			&& (ChildCoroutineErrors.Num() > 0 || TrueAwaitables.Num() >= sizeof...(InnerAwaitableTypes)))
		{
			Handle.resume();
		}
	}
};

template <typename... AwaitableTypes>
TAllOfAwaitable(AwaitableTypes&&...) -> TAllOfAwaitable<AwaitableTypes...>;


namespace Awaitables
{
	template <typename... AwaitableTypes>
	auto AllOf(AwaitableTypes&&... Awaitables)
	{
		return TAllOfAwaitable{Forward<AwaitableTypes>(Awaitables)...};
	}
}
