// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "PaperUnreal/Core/WeakCoroutine/TypeTraits.h"
#include "Algo/AllOf.h"


namespace WeakCoroutineDetails
{
	template <typename T, typename WeakListContainerType>
	concept CWeakListAddable = requires(T Arg, WeakListContainerType Container)
	{
		Container.AddToWeakList(Arg);
	};
}


class FWeakCoroutineContext2;
class FWeakCoroutinePromiseType;


class FWeakCoroutine2
{
public:
	using promise_type = FWeakCoroutinePromiseType;

	FWeakCoroutine2(std::coroutine_handle<FWeakCoroutinePromiseType> InHandle)
		: Handle(InHandle)
	{
	}

	void Init(
		TUniquePtr<TUniqueFunction<FWeakCoroutine2(FWeakCoroutineContext2&)>> Captures,
		TUniquePtr<FWeakCoroutineContext2> Context);

	void Resume();

private:
	std::coroutine_handle<FWeakCoroutinePromiseType> Handle;
};


class FWeakCoroutinePromiseType
{
public:
	FWeakCoroutine2 get_return_object()
	{
		return std::coroutine_handle<FWeakCoroutinePromiseType>::from_promise(*this);
	}

	std::suspend_always initial_suspend() const
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

	template <typename AnyType>
	auto await_transform(AnyType&& Any)
	{
		return await_transform(operator co_await(Forward<AnyType>(Any)));
	}

	template <CAwaitable AwaitableType>
	auto await_transform(AwaitableType&& Awaitable);

	bool IsValid() const
	{
		return Algo::AllOf(WeakList, [](const auto& Each) { return Each(); });
	}

	void AddToWeakList(UObject* Object)
	{
		WeakList.Add([Weak = TWeakObjectPtr{Object}]() { return Weak.IsValid(); });
	}

	template <typename T>
	void AddToWeakList(TScriptInterface<T> Interface)
	{
		AddToWeakList(Interface.GetObject());
	}

	template <typename T>
	void AddToWeakList(const TSharedPtr<T>& Object)
	{
		WeakList.Add([Weak = TWeakPtr<T>{Object}]() { return Weak.IsValid(); });
	}

	template <typename T>
	void AddToWeakList(const TSharedRef<T>& Object)
	{
		WeakList.Add([Weak = TWeakPtr<T>{Object}]() { return Weak.IsValid(); });
	}

	template <typename T>
	void AddToWeakList(const TWeakPtr<T>& Object)
	{
		WeakList.Add([Weak = Object]() { return Weak.IsValid(); });
	}

	template <typename T>
	void AddToWeakList(const TWeakObjectPtr<T>& Object)
	{
		WeakList.Add([Weak = Object]() { return Weak.IsValid(); });
	}

private:
	friend class FWeakCoroutine2;
	TArray<TFunction<bool()>> WeakList;
	TUniquePtr<FWeakCoroutineContext2> Context;
	TUniquePtr<TUniqueFunction<FWeakCoroutine2(FWeakCoroutineContext2&)>> Captures;
};


class FWeakCoroutineContext2
{
public:
	template <typename T>
	decltype(auto) AbortIfNotValid(T&& Weak)
	{
		Handle.promise().AddToWeakList(Weak);
		return Forward<T>(Weak);
	}

private:
	friend class FWeakCoroutine2;
	std::coroutine_handle<FWeakCoroutinePromiseType> Handle;
};


template <typename WrappedAwaitableType>
class TWeakAwaitable2
{
public:
	using ReturnType = decltype(std::declval<WrappedAwaitableType>().await_resume());

	template <typename T> requires std::is_convertible_v<T, WrappedAwaitableType>
	TWeakAwaitable2(T&& Awaitable)
		: WrappedAwaitable(Forward<T>(Awaitable))
	{
	}

	bool await_ready() const
	{
		return WrappedAwaitable.await_ready();
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutinePromiseType> InHandle)
	{
		Handle = InHandle;

		struct FWeakCheckingHandle
		{
			std::coroutine_handle<FWeakCoroutinePromiseType> Handle;

			void resume() const
			{
				Handle.promise().IsValid() ? Handle.resume() : Handle.destroy();
			}

			void destroy() const
			{
				Handle.destroy();
			}
		};

		WrappedAwaitable.await_suspend(FWeakCheckingHandle{Handle});
	}

	ReturnType await_resume()
	{
		if constexpr (WeakCoroutineDetails::CWeakListAddable<ReturnType, FWeakCoroutinePromiseType>)
		{
			ReturnType Ret = WrappedAwaitable.await_resume();
			Handle.promise().AddToWeakList(Ret);
			return Ret;
		}
		else
		{
			return WrappedAwaitable.await_resume();
		}
	}

private:
	WrappedAwaitableType WrappedAwaitable;
	std::coroutine_handle<FWeakCoroutinePromiseType> Handle;
};


inline void FWeakCoroutine2::Init(
	TUniquePtr<TUniqueFunction<FWeakCoroutine2(FWeakCoroutineContext2&)>> Captures,
	TUniquePtr<FWeakCoroutineContext2> Context)
{
	Handle.promise().Captures = MoveTemp(Captures);
	Handle.promise().Context = MoveTemp(Context);
	Handle.promise().Context->Handle = Handle;
}

inline void FWeakCoroutine2::Resume()
{
	Handle.resume();
}


template <CAwaitable AwaitableType>
auto FWeakCoroutinePromiseType::await_transform(AwaitableType&& Awaitable)
{
	if constexpr (TIsInstantiationOf_V<AwaitableType, TCancellableFutureAwaitable>)
	{
		using NoErrorAwaitableType = TErrorRemovedCancellableFutureAwaitable<std::decay_t<AwaitableType>>;
		return TWeakAwaitable2<NoErrorAwaitableType>{NoErrorAwaitableType{Forward<AwaitableType>(Awaitable)}};
	}
	else
	{
		return TWeakAwaitable2<std::decay_t<AwaitableType>>{Forward<AwaitableType>(Awaitable)};
	}
}


template <typename... Types>
auto WithError(TCancellableFuture<Types...>&& Future)
{
	return TIdentityAwaitable<TCancellableFutureAwaitable<Types...>>{operator co_await(MoveTemp(Future))};
}


template <typename FuncType>
void RunWeakCoroutine2(FuncType&& Func)
{
	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<FWeakCoroutine2(FWeakCoroutineContext2&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<FWeakCoroutineContext2>();

	FWeakCoroutine2 WeakCoroutine = (*LambdaCaptures)(*Context);
	WeakCoroutine.Init(MoveTemp(LambdaCaptures), MoveTemp(Context));
	WeakCoroutine.Resume();
}


template <typename FuncType>
void RunWeakCoroutine2(const auto& Lifetime, FuncType&& Func)
{
	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<FWeakCoroutine2(FWeakCoroutineContext2&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<FWeakCoroutineContext2>();
	auto ContextPtr = Context.Get();

	FWeakCoroutine2 WeakCoroutine = (*LambdaCaptures)(*Context);
	WeakCoroutine.Init(MoveTemp(LambdaCaptures), MoveTemp(Context));
	ContextPtr->AbortIfNotValid(Lifetime);
	WeakCoroutine.Resume();
}
