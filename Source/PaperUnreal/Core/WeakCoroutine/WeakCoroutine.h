// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "CancellableFuture.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"


namespace WeakCoroutineDetails
{
	template <typename T, typename WeakListContainerType>
	concept CWeakListAddable = requires(T Arg, WeakListContainerType Container)
	{
		Container.AddToWeakList(Arg);
	};
}


template <typename, typename...>
class TWeakCoroutineContext;

template <typename, typename...>
class TWeakCoroutinePromiseType;


template <typename T, typename... ErrorTypes>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T, ErrorTypes...>;
	using ContextType = TWeakCoroutineContext<T, ErrorTypes...>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle)
	{
	}

	void Init(
		TUniquePtr<TUniqueFunction<TWeakCoroutine(TWeakCoroutineContext<T, ErrorTypes...>&)>> Captures,
		TUniquePtr<TWeakCoroutineContext<T, ErrorTypes...>> Context);

	void Resume();

	template <typename U> requires std::is_convertible_v<U, TWeakCoroutine>
	friend auto operator co_await(U&& Coroutine)
	{
		return operator co_await(MoveTemp(*Coroutine.Future));
	}

private:
	std::coroutine_handle<promise_type> Handle;
	TOptional<TCancellableFuture<T, ErrorTypes...>> Future;
};


template <typename Derived, typename T, typename... ErrorTypes>
class TWeakCoroutinePromiseTypeBase
{
public:
	using ReturnObjectType = TWeakCoroutine<T, ErrorTypes...>;

	ReturnObjectType get_return_object()
	{
		return std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
	}

	std::suspend_always initial_suspend() const
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

protected:
	friend class TWeakCoroutine<T, ErrorTypes...>;

	TArray<TFunction<bool()>> WeakList;
	TUniquePtr<TWeakCoroutineContext<T, ErrorTypes...>> Context;
	TUniquePtr<TUniqueFunction<ReturnObjectType(TWeakCoroutineContext<T, ErrorTypes...>&)>> Captures;
	TOptional<TCancellablePromise<T, ErrorTypes...>> Promise;
};


template <typename T, typename... ErrorTypes>
class TWeakCoroutinePromiseType
	: public TWeakCoroutinePromiseTypeBase<TWeakCoroutinePromiseType<T, ErrorTypes...>, T, ErrorTypes...>
{
public:
	template <typename U>
	void return_value(U&& Value)
	{
		this->Promise->SetValue(Forward<U>(Value));
	}
};

template <typename... ErrorTypes>
class TWeakCoroutinePromiseType<void, ErrorTypes...>
	: public TWeakCoroutinePromiseTypeBase<TWeakCoroutinePromiseType<void, ErrorTypes...>, void, ErrorTypes...>
{
public:
	void return_void()
	{
		this->Promise->SetValue();
	}
};


template <typename T, typename... ErrorTypes>
class TWeakCoroutineContext
{
public:
	TWeakCoroutineContext() = default;
	TWeakCoroutineContext(const TWeakCoroutineContext&) = delete;
	TWeakCoroutineContext& operator=(const TWeakCoroutineContext&) = delete;
	
	template <typename U>
	decltype(auto) AbortIfNotValid(U&& Weak)
	{
		Handle.promise().AddToWeakList(Weak);
		return Forward<U>(Weak);
	}

private:
	friend class TWeakCoroutine<T, ErrorTypes...>;
	std::coroutine_handle<TWeakCoroutinePromiseType<T, ErrorTypes...>> Handle;
};


template <typename PromiseType, typename WrappedAwaitableType>
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

	void await_suspend(std::coroutine_handle<PromiseType> InHandle)
	{
		Handle = InHandle;

		struct FWeakCheckingHandle
		{
			std::coroutine_handle<PromiseType> Handle;

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
		if constexpr (WeakCoroutineDetails::CWeakListAddable<ReturnType, PromiseType>)
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
	std::coroutine_handle<PromiseType> Handle;
};


template <typename T, typename... ErrorTypes>
void TWeakCoroutine<T, ErrorTypes...>::Init(
	TUniquePtr<TUniqueFunction<TWeakCoroutine(TWeakCoroutineContext<T, ErrorTypes...>&)>> Captures,
	TUniquePtr<TWeakCoroutineContext<T, ErrorTypes...>> Context)
{
	Handle.promise().Captures = MoveTemp(Captures);
	Handle.promise().Context = MoveTemp(Context);
	Handle.promise().Context->Handle = Handle;

	auto [NewPromise, NewFuture] = MakePromise<T, ErrorTypes...>();
	Handle.promise().Promise.Emplace(MoveTemp(NewPromise));
	Future.Emplace(MoveTemp(NewFuture));
}

template <typename T, typename... ErrorTypes>
void TWeakCoroutine<T, ErrorTypes...>::Resume()
{
	Handle.resume();
}


template <typename Derived, typename T, typename... ErrorTypes>
template <CAwaitable AwaitableType>
auto TWeakCoroutinePromiseTypeBase<Derived, T, ErrorTypes...>::await_transform(AwaitableType&& Awaitable)
{
	if constexpr (TIsInstantiationOf_V<AwaitableType, TCancellableFutureAwaitable>)
	{
		using NoErrorAwaitableType = TErrorRemovedCancellableFutureAwaitable<std::decay_t<AwaitableType>>;
		return TWeakAwaitable2<Derived, NoErrorAwaitableType>{NoErrorAwaitableType{Forward<AwaitableType>(Awaitable)}};
	}
	else
	{
		return TWeakAwaitable2<Derived, std::decay_t<AwaitableType>>{Forward<AwaitableType>(Awaitable)};
	}
}


template <typename... Types>
auto WithError(TCancellableFuture<Types...>&& Future)
{
	return TIdentityAwaitable<TCancellableFutureAwaitable<Types...>>{operator co_await(MoveTemp(Future))};
}


template <typename FuncType>
auto RunWeakCoroutine(FuncType&& Func)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;
	using ContextType = typename CoroutineType::ContextType;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<CoroutineType(ContextType&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<ContextType>();

	CoroutineType WeakCoroutine = (*LambdaCaptures)(*Context);
	WeakCoroutine.Init(MoveTemp(LambdaCaptures), MoveTemp(Context));
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


template <typename FuncType>
auto RunWeakCoroutine(const auto& Lifetime, FuncType&& Func)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;
	using ContextType = typename CoroutineType::ContextType;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<CoroutineType(ContextType&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<ContextType>();
	auto ContextPtr = Context.Get();

	CoroutineType WeakCoroutine = (*LambdaCaptures)(*Context);
	WeakCoroutine.Init(MoveTemp(LambdaCaptures), MoveTemp(Context));
	ContextPtr->AbortIfNotValid(Lifetime);
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


using FWeakCoroutine = TWeakCoroutine<void>;
using FWeakCoroutineContext = TWeakCoroutineContext<void>;
