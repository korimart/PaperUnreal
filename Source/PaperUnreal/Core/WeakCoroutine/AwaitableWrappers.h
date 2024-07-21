// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <source_location>

#include "CoreMinimal.h"
#include "PaperUnreal/Core/Utils.h"
#include "PaperUnreal/Core/WeakCoroutine/TypeTraits.h"
#include "PaperUnreal/Core/WeakCoroutine/CancellableFuture.h"


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


template <typename FutureAwaitableType>
class TErrorRemovedCancellableFutureAwaitable
{
public:
	TErrorRemovedCancellableFutureAwaitable(FutureAwaitableType&& Awaitable, std::source_location Location)
		: FutureAwaitable(MoveTemp(Awaitable)), SourceLocation(Location)
	{
	}

	TErrorRemovedCancellableFutureAwaitable(const TErrorRemovedCancellableFutureAwaitable&) = delete;
	TErrorRemovedCancellableFutureAwaitable& operator=(const TErrorRemovedCancellableFutureAwaitable&) = delete;

	TErrorRemovedCancellableFutureAwaitable(TErrorRemovedCancellableFutureAwaitable&&) = default;
	TErrorRemovedCancellableFutureAwaitable& operator=(TErrorRemovedCancellableFutureAwaitable&&) = delete;

	bool await_ready() const
	{
		return FutureAwaitable.await_ready();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		struct FErrorCheckingHandle
		{
			TErrorRemovedCancellableFutureAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const
			{
				if (This->FutureAwaitable.Peek())
				{
					Handle.resume();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AbortOnError %hs %hs %d %d"),
						This->SourceLocation.file_name(),
						This->SourceLocation.function_name(),
						This->SourceLocation.line(),
						This->SourceLocation.column());
					
					Handle.destroy();
				}
			}
		};

		FutureAwaitable.await_suspend(FErrorCheckingHandle{this, Forward<HandleType>(Handle)});
	}

	auto await_resume()
	{
		return FutureAwaitable.await_resume().Get();
	}

private:
	FutureAwaitableType FutureAwaitable;
	std::source_location SourceLocation;
};


template <typename... AwaitableTypes>
class TAnyOfAwaitable
{
public:
	TAnyOfAwaitable(AwaitableTypes&&... InAwaitables)
		: Awaitables(MoveTemp(InAwaitables)...)
	{
	}

	TAnyOfAwaitable(const TAnyOfAwaitable&) = delete;
	TAnyOfAwaitable& operator=(const TAnyOfAwaitable&) = delete;

	TAnyOfAwaitable(TAnyOfAwaitable&&) = default;
	TAnyOfAwaitable& operator=(TAnyOfAwaitable&&) = delete;

	bool await_ready()
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
	TOptional<int32> ReturnValue;
};


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


template <typename AnyType>
auto AbortOnError(AnyType&& Awaitable, std::source_location Location = std::source_location::current())
{
	return AbortOnError(operator co_await(Forward<AnyType>(Awaitable)), Location);
}

template <typename... Types>
auto AbortOnError(TCancellableFutureAwaitable<Types...>&& Awaitable, std::source_location Location = std::source_location::current())
{
	return TErrorRemovedCancellableFutureAwaitable<TCancellableFutureAwaitable<Types...>>{MoveTemp(Awaitable), Location};
}


template <typename... MaybeAwaitableTypes>
auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
{
	using namespace AwaitableWrapperDetails;
	return AnyOfImpl(AwaitableOrIdentity(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...);
}
