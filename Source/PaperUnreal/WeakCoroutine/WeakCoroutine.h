// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AbortablePromise.h"
#include "AwaitableWrappers.h"
#include "CancellableFuture.h"
#include "LoggingPromise.h"
#include "MinimalCoroutine.h"
#include "TypeTraits.h"
#include "WeakPromise.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "WeakCoroutine.generated.h"


UCLASS()
class UWeakCoroutineError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UWeakCoroutineError* InvalidCoroutine()
	{
		return NewError<UWeakCoroutineError>(TEXT("AbortIfInvalid로 등록된 오브젝트가 Valid 하지 않음"));
	}

	static UWeakCoroutineError* ExplicitAbort()
	{
		return NewError<UWeakCoroutineError>(TEXT("누군가가 코루틴에 대해 Abort를 호출했습니다"));
	}
};


namespace WeakCoroutine_Private
{
	template <typename AwaitableType, typename InAllowedErrorTypeList>
	class TWithErrorAwaitable : public TIdentityAwaitable<AwaitableType>
	{
	public:
		using AllowedErrorTypeList = InAllowedErrorTypeList;
		using TIdentityAwaitable<AwaitableType>::TIdentityAwaitable;
	};
}


template <typename>
class TWeakCoroutine;

template <typename>
class TWeakCoroutinePromiseType;


template <typename T>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle), PromiseLife(Handle.promise().Life)
	{
	}

	void Init(TUniquePtr<TUniqueFunction<TWeakCoroutine()>> Captures)
	{
		Handle.promise().Captures = MoveTemp(Captures);

		auto [NewPromise, NewFuture] = MakePromise<T>();
		Handle.promise().Promise.Emplace(MoveTemp(NewPromise));
		CoroutineRet.Emplace(MoveTemp(NewFuture));
	}

	void Resume()
	{
		Handle.resume();
	}

	void Abort()
	{
		if (PromiseLife.IsValid())
		{
			Handle.promise().Abort();
		}
	}

	void AddToWeakList(const UObject* Object)
	{
		if (PromiseLife.IsValid())
		{
			Handle.promise().AddToWeakList(Object);
		}
	}

	TCancellableFuture<T> ReturnValue()
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
	TWeakPtr<std::monostate> PromiseLife;
	TOptional<TCancellableFuture<T>> CoroutineRet;
};


template <typename Derived, typename T>
class TWeakCoroutinePromiseTypeBase : public FLoggingPromise
                                      , public TAbortablePromise<TWeakCoroutinePromiseTypeBase<Derived, T>>
                                      , public TWeakPromise<TWeakCoroutinePromiseTypeBase<Derived, T>>
{
public:
	TSharedRef<std::monostate> Life = MakeShared<std::monostate>();

	using ReturnObjectType = TWeakCoroutine<T>;

	ReturnObjectType get_return_object()
	{
		return std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
	}

	std::suspend_always initial_suspend() const
	{
		return {};
	}

	std::suspend_never final_suspend() noexcept
	{
		ClearErrors();
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

	template <typename... _>
	auto await_transform(WeakCoroutine_Private::TWithErrorAwaitable<_...>&& Awaitable)
	{
		using AllowedErrorTypeList = typename std::decay_t<decltype(Awaitable)>::AllowedErrorTypeList;

		return MoveTemp(Awaitable.Inner())
			| Awaitables::AbortIfInvalidPromise()
			| Awaitables::AbortIfErrorNotIn<AllowedErrorTypeList>()
			| Awaitables::ReturnAsAbortPtr(*this)
			| Awaitables::CaptureSourceLocation();
	}

	template <CAwaitable AwaitableType>
	auto await_transform(AwaitableType&& Awaitable)
	{
		return Forward<AwaitableType>(Awaitable)
			| Awaitables::AbortIfInvalidPromise()
			| Awaitables::AbortIfError()
			| Awaitables::ReturnAsAbortPtr(*this)
			| Awaitables::CaptureSourceLocation();
	}

protected:
	friend class TWeakCoroutine<T>;

	TUniquePtr<TUniqueFunction<ReturnObjectType()>> Captures;
	TOptional<TCancellablePromise<T>> Promise;

private:
	friend struct TAbortablePromise<TWeakCoroutinePromiseTypeBase>;

	void OnAbortRequested()
	{
		AddError(UWeakCoroutineError::ExplicitAbort());
	}

	friend struct TWeakPromise<TWeakCoroutinePromiseTypeBase>;

	void OnAbortByInvalidity()
	{
		AddError(UWeakCoroutineError::InvalidCoroutine());
	}
};


template <typename T>
class TWeakCoroutinePromiseType
	: public TWeakCoroutinePromiseTypeBase<TWeakCoroutinePromiseType<T>, T>
{
public:
	template <typename U>
	void return_value(U&& Value)
	{
		this->Promise->SetValue(Forward<U>(Value));
	}
};


template <>
class TWeakCoroutinePromiseType<void>
	: public TWeakCoroutinePromiseTypeBase<TWeakCoroutinePromiseType<void>, void>
{
public:
	void return_void()
	{
		this->Promise->SetValue();
	}
};


template <typename FuncType>
auto RunWeakCoroutine(FuncType&& Func)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<CoroutineType()>>(Forward<FuncType>(Func));

	CoroutineType WeakCoroutine = (*LambdaCaptures)();
	WeakCoroutine.Init(MoveTemp(LambdaCaptures));
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


template <typename FuncType>
auto RunWeakCoroutine(const UObject* Lifetime, FuncType&& Func)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<CoroutineType()>>(Forward<FuncType>(Func));

	CoroutineType WeakCoroutine = (*LambdaCaptures)();
	WeakCoroutine.Init(MoveTemp(LambdaCaptures));
	WeakCoroutine.AddToWeakList(Lifetime);
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


template <typename FuncType, typename... ArgTypes>
auto RunWeakCoroutineNoCaptures(const FuncType& Func, ArgTypes&&... Args)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;

	CoroutineType WeakCoroutine = Func(Forward<ArgTypes>(Args)...);
	WeakCoroutine.Init(nullptr);
	WeakCoroutine.Resume();
	return WeakCoroutine;
}


template <typename... AllowedErrorTypes, typename MaybeAwaitableType>
auto WithError(MaybeAwaitableType&& MaybeAwaitable)
{
	using namespace WeakCoroutine_Private;

	using AllowedErrorTypeList = std::conditional_t
	<
		sizeof...(AllowedErrorTypes) == 0,
		void,
		TTypeList<AllowedErrorTypes...>
	>;

	auto Awaitable = AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable));
	return TWithErrorAwaitable<decltype(Awaitable), AllowedErrorTypeList>{MoveTemp(Awaitable)};
}


using FWeakCoroutine = TWeakCoroutine<void>;
