// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/Core/Utils.h"
#include "PaperUnreal/Core/WeakCoroutine/TypeTraits.h"


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
	TErrorRemovedCancellableFutureAwaitable(FutureAwaitableType&& Awaitable)
		: FutureAwaitable(MoveTemp(Awaitable))
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
				This->FutureAwaitable.Peek().GetIndex() == 0 ? Handle.resume() : Handle.destroy();
			}
		};

		FutureAwaitable.await_suspend(FErrorCheckingHandle{this, Forward<HandleType>(Handle)});
	}

	auto await_resume()
	{
		return FutureAwaitable.await_resume().template Get<typename FutureAwaitableType::ResultType>();
	}

private:
	FutureAwaitableType FutureAwaitable;
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

	bool await_ready() const
	{
		return Awaitables.ApplyAfter([](const auto&... Args) { return (Args.await_ready() || ...); });
	}

	template <typename HandleType>
	void await_suspend(const HandleType& Handle)
	{
		auto bComplete = MakeShared<bool>(false);

		struct FCompleteCheckingHandle
		{
			TSharedRef<bool> bComplete;
			std::decay_t<HandleType> Handle;

			void Resume()
			{
				if (!*bComplete)
				{
					*bComplete = true;
					Handle.resume();
				}
			}
		};

		const auto WaitForCompletion = [](auto Awaitable, FCompleteCheckingHandle Handle) -> FMinimalCoroutine
		{
			auto F = Finally([&]() { Handle.Resume(); });
			co_await Awaitable;
		};

		MoveTemp(Awaitables).ApplyAfter([&](auto&&... Args)
		{
			(WaitForCompletion(MoveTemp(Args), {bComplete, Handle}), ...);
		});
	}

	void await_resume()
	{
	}

private:
	TTuple<AwaitableTypes...> Awaitables;
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
		return TAnyOfAwaitable<AwaitableTypes...>{MoveTemp(Awaitables)...};
	}
}


template <typename... MaybeAwaitableTypes>
auto AnyOf(MaybeAwaitableTypes&&... MaybeAwaitables)
{
	using namespace AwaitableWrapperDetails;
	return AnyOfImpl(AwaitableOrIdentity(Forward<MaybeAwaitableTypes>(MaybeAwaitables))...);
}
