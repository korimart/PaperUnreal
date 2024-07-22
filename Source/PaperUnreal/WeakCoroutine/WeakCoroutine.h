// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"
#include "PaperUnreal/GameFramework2/Utils.h"


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

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle, const TSharedRef<bool>& bFinished)
		: Handle(InHandle), bCoroutineFinished(bFinished)
	{
	}

	// TODO context doesn't need to be a pointer
	void Init(
		TUniquePtr<TUniqueFunction<TWeakCoroutine(TWeakCoroutineContext<T, ErrorTypes...>&)>> Captures,
		TUniquePtr<TWeakCoroutineContext<T, ErrorTypes...>> Context);

	void Resume();

	void Abort();

	TCancellableFuture<T, ErrorTypes...> ReturnValue()
	{
		check(CoroutineRet.IsSet());
		auto Ret = MoveTemp(*CoroutineRet);
		CoroutineRet.Reset();
		return Ret;
	}

	template <typename U> requires std::is_convertible_v<U, TWeakCoroutine>
	friend auto operator co_await(U&& Coroutine)
	{
		return operator co_await(Coroutine.ReturnValue());
	}

private:
	std::coroutine_handle<promise_type> Handle;
	TSharedRef<bool> bCoroutineFinished;
	TOptional<TCancellableFuture<T, ErrorTypes...>> CoroutineRet;
};


template <typename Derived, typename T, typename... ErrorTypes>
class TWeakCoroutinePromiseTypeBase
{
public:
	using ReturnObjectType = TWeakCoroutine<T, ErrorTypes...>;

	ReturnObjectType get_return_object()
	{
		return
		{
			std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this)),
			bCoroutineFinished,
		};
	}

	std::suspend_always initial_suspend() const
	{
		return {};
	}

	std::suspend_never final_suspend() const noexcept
	{
		*bCoroutineFinished = true;
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

	void Abort()
	{
		*bCoroutineFinished = true;
	}

	bool IsValid() const
	{
		return !*bCoroutineFinished && Algo::AllOf(WeakList, [](const auto& Each) { return Each(); });
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
	TSharedRef<bool> bCoroutineFinished = MakeShared<bool>(false);
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
		check(AllValid(Weak));
		Handle.promise().AddToWeakList(Weak);
		return Forward<U>(Weak);
	}

private:
	friend class TWeakCoroutine<T, ErrorTypes...>;
	std::coroutine_handle<TWeakCoroutinePromiseType<T, ErrorTypes...>> Handle;
};


template <typename PromiseType, typename WrappedAwaitableType>
class TWeakAwaitable
{
public:
	using ReturnType = decltype(std::declval<WrappedAwaitableType>().await_resume());

	template <typename T> requires std::is_convertible_v<T, WrappedAwaitableType>
	TWeakAwaitable(T&& Awaitable)
		: WrappedAwaitable(Forward<T>(Awaitable))
	{
	}

	bool await_ready() const
	{
		return false;
	}

	bool await_suspend(std::coroutine_handle<PromiseType> InHandle)
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

		if (WrappedAwaitable.await_ready())
		{
			return false;
		}

		WrappedAwaitable.await_suspend(FWeakCheckingHandle{Handle});
		return true;
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
	CoroutineRet.Emplace(MoveTemp(NewFuture));
}

template <typename T, typename... ErrorTypes>
void TWeakCoroutine<T, ErrorTypes...>::Resume()
{
	Handle.resume();
}

template <typename T, typename... ErrorTypes>
void TWeakCoroutine<T, ErrorTypes...>::Abort()
{
	if (!*bCoroutineFinished)
	{
		Handle.promise().Abort();
	}
}


template <typename Derived, typename T, typename... ErrorTypes>
template <CAwaitable AwaitableType>
auto TWeakCoroutinePromiseTypeBase<Derived, T, ErrorTypes...>::await_transform(AwaitableType&& Awaitable)
{
	return TWeakAwaitable<Derived, std::decay_t<AwaitableType>>{Forward<AwaitableType>(Awaitable)};
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


template <typename FuncType, typename... ArgTypes>
auto RunWeakCoroutineNoCaptures(const FuncType& Func, ArgTypes&&... Args)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;
	using ContextType = typename CoroutineType::ContextType;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto Context = MakeUnique<ContextType>();

	CoroutineType WeakCoroutine = Func(*Context, Forward<ArgTypes>(Args)...);
	WeakCoroutine.Init(nullptr, MoveTemp(Context));
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


using FWeakCoroutine = TWeakCoroutine<void>;
using FWeakCoroutineContext = TWeakCoroutineContext<void>;
