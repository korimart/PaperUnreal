// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "TypeTraits.h"


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

private:
	AwaitableType Awaitable;
};


// TODO 필터 대상이 NonErrorReporting인지 체크
template <typename... AwaitableTypes>
class TAnyOfAwaitable
{
public:
	TAnyOfAwaitable(AwaitableTypes&&... InAwaitables)
		: Awaitables(MoveTemp(InAwaitables)...)
	{
		// TODO 이거 만족시켜야 됨
		// static_assert(CErrorReportingAwaitable<TAnyOfAwaitable>);
	}

	TAnyOfAwaitable(const TAnyOfAwaitable&) = delete;
	TAnyOfAwaitable& operator=(const TAnyOfAwaitable&) = delete;

	TAnyOfAwaitable(TAnyOfAwaitable&&) = default;
	TAnyOfAwaitable& operator=(TAnyOfAwaitable&&) = delete;

	bool await_ready() const
	{
		const auto FindReadyAwaitable = [&]<std::size_t Index, std::size_t... Indices>(
			auto& Self, std::index_sequence<Index, Indices...>) -> TOptional<int32>
		{
			if (Awaitables.template Get<Index>().await_ready())
			{
				return Index;
			}

			if constexpr (sizeof...(Indices) > 0)
			{
				return Self(Self, std::index_sequence<Indices...>{});
			}
			else
			{
				return {};
			}
		};

		ReturnValue = FindReadyAwaitable(FindReadyAwaitable, std::index_sequence_for<AwaitableTypes...>{});

		return !!ReturnValue;
	}

	template <typename HandleType>
	void await_suspend(const HandleType& Handle)
	{
		struct FCompletionReporter
		{
			int32 Index;
			HandleType Handle;
			TAnyOfAwaitable* This;
			TSharedRef<bool> bComplete;

			void OnComplete()
			{
				if (!*bComplete)
				{
					*bComplete = true;
					This->ReturnValue = Index;
					Handle.resume();
				}
			}
		};

		struct FNoCompletionReporter
		{
			HandleType Handle;
			TSharedRef<bool> bComplete;

			~FNoCompletionReporter()
			{
				if (!*bComplete)
				{
					Handle.resume();
				}
			}
		};

		const auto WaitForCompletion = [](
			auto Awaitable,
			FCompletionReporter Reporter,
			TSharedRef<FNoCompletionReporter>) -> FMinimalCoroutine
		{
			co_await Awaitable;
			Reporter.OnComplete();
		};

		auto bComplete = MakeShared<bool>(false);
		auto NoCompletionReporter = MakeShared<FNoCompletionReporter>(Handle, bComplete);

		const auto RunCoroutines = [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
		{
			(WaitForCompletion(
				MoveTemp(Awaitables.template Get<Indices>()),
				{Indices, Handle, this, bComplete},
				NoCompletionReporter), ...);
		};

		RunCoroutines(std::index_sequence_for<AwaitableTypes...>());
	}

	TOptional<int32> await_resume()
	{
		return ReturnValue;
	}

private:
	TTuple<AwaitableTypes...> Awaitables;
	mutable TOptional<int32> ReturnValue;
};


// TODO 필터 대상이 NonErrorReporting인지 체크
template <typename ValueStreamType, typename PredicateType>
class TFilteringAwaitable
{
public:
	template <typename Stream, typename Pred>
	TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
		: ValueStream(Forward<Stream>(ValueStream)), Predicate(Forward<Pred>(Predicate))
	{
		static_assert(CErrorReportingAwaitable<TFilteringAwaitable>);
	}
	
	bool await_ready() const
	{
		return false;
	}

	void await_suspend(auto Handle)
	{
		[](TFilteringAwaitable& This, auto Handle) -> FMinimalCoroutine
		{
			while (true)
			{
				auto Result = co_await This.ValueStream;
				if (Result.Failed() || This.Predicate(Result.GetResult()))
				{
					This.Ret = MoveTemp(Result);
					break;
				}
			}

			Handle.resume();
			
		}(*this, Handle);
	}

	auto await_resume()
	{
		return MoveTemp(Ret.GetValue());
	}
	
	bool Failed() const
	{
		return Ret->Failed();
	}

private:
	ValueStreamType ValueStream;
	PredicateType Predicate;
	TOptional<decltype(operator co_await(std::declval<std::decay_t<ValueStreamType>&>()).await_resume())> Ret;
};

template <typename Stream, typename Pred>
TFilteringAwaitable(Stream&& ValueStream, Pred&& Predicate)
	-> TFilteringAwaitable<std::conditional_t<std::is_lvalue_reference_v<Stream>, Stream, std::decay_t<Stream>>, std::decay_t<Pred>>;


namespace AwaitableWrapperDetails
{
	template <typename T>
	decltype(auto) AwaitableOrIdentity(T&& MaybeAwaitable)
	{
		if constexpr (CAwaitable<T>)
		{
			return Forward<T>(MaybeAwaitable);
		}
		else
		{
			return operator co_await(Forward<T>(MaybeAwaitable));
		}
	}

	template <typename... AwaitableTypes>
	TAnyOfAwaitable<AwaitableTypes...> AnyOfImpl(AwaitableTypes... Awaitables)
	{
		return {MoveTemp(Awaitables)...};
	}
}


template <typename... MaybeAwaitableTypes>
auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
{
	using namespace AwaitableWrapperDetails;
	return AnyOfImpl(AwaitableOrIdentity(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...);
}


template <typename ValueStreamType, typename PredicateType>
auto Filter(ValueStreamType&& Stream, PredicateType&& Predicate)
{
	return TFilteringAwaitable{Forward<ValueStreamType>(Stream), Forward<PredicateType>(Predicate)};
}