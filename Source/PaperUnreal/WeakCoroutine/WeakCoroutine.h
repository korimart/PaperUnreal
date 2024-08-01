// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AbortablePromise.h"
#include "AwaitablePromise.h"
#include "AwaitableWrappers.h"
#include "CancellableFuture.h"
#include "LoggingPromise.h"
#include "MinimalCoroutine.h"
#include "TypeTraits.h"
#include "WeakPromise.h"
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


template <typename AwaitableType, typename InAllowedErrorTypeList>
class TCatchAwaitable : public TIdentityAwaitable<AwaitableType>
{
public:
	using AllowedErrorTypeList = InAllowedErrorTypeList;
	using TIdentityAwaitable<AwaitableType>::TIdentityAwaitable;

	bool await_ready()
	{
		// 이 Awaitable은 진짜 awaitable이 아님 사라져야 함
		check(false);
		return false;
	}
};


template <typename>
class TWeakCoroutinePromiseType;


template <typename T>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle), PromiseLife(Handle.promise().Life), PromiseReturn(Handle.promise().GetFuture())
	{
	}

	void Init(TUniquePtr<TUniqueFunction<TWeakCoroutine()>> Captures)
	{
		Handle.promise().Captures = MoveTemp(Captures);
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
		return MoveTemp(PromiseReturn);
	}

	friend auto operator co_await(TWeakCoroutine& WeakCoroutine)
	{
		return operator co_await(WeakCoroutine.ReturnValue());
	}
	
	friend auto operator co_await(TWeakCoroutine&& WeakCoroutine)
	{
		return operator co_await(WeakCoroutine.ReturnValue());
	}

private:
	std::coroutine_handle<promise_type> Handle;
	TWeakPtr<std::monostate> PromiseLife;
	TCancellableFuture<T> PromiseReturn;
};


template <typename T>
class TWeakCoroutinePromiseType : public FLoggingPromise
                                  , public TAbortablePromise<TWeakCoroutinePromiseType<T>>
                                  , public TWeakPromise<TWeakCoroutinePromiseType<T>>
                                  , public TAwaitablePromise<T>
{
public:
	TSharedRef<std::monostate> Life = MakeShared<std::monostate>();

	using ReturnObjectType = TWeakCoroutine<T>;

	ReturnObjectType get_return_object()
	{
		return std::coroutine_handle<TWeakCoroutinePromiseType>::from_promise(*this);
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

	template <typename WithErrorAwaitableType>
		requires TIsInstantiationOf_V<WithErrorAwaitableType, TCatchAwaitable>
	auto await_transform(WithErrorAwaitableType&& Awaitable)
	{
		using AllowedErrorTypeList = typename std::decay_t<WithErrorAwaitableType>::AllowedErrorTypeList;

		return Forward<WithErrorAwaitableType>(Awaitable).Inner()
			| Awaitables::AbortIfInvalidPromise()
			| Awaitables::AbortIfErrorNotIn<AllowedErrorTypeList>()
			| Awaitables::ReturnAsAbortPtr(*this)
			| Awaitables::CaptureSourceLocation();
	}

	template <CAwaitable AwaitableType>
		requires !TIsInstantiationOf_V<AwaitableType, TCatchAwaitable>
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

private:
	friend struct TAbortablePromise<TWeakCoroutinePromiseType>;

	void OnAbortRequested()
	{
		AddError(UWeakCoroutineError::ExplicitAbort());
	}

	friend struct TWeakPromise<TWeakCoroutinePromiseType>;

	void OnAbortByInvalidity()
	{
		AddError(UWeakCoroutineError::InvalidCoroutine());
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


struct FCatchAllAdaptor : TAwaitableAdaptorBase<FCatchAllAdaptor>
{
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, FCatchAllAdaptor)
	{
		return TCatchAwaitable<AwaitableType, void>{Forward<AwaitableType>(Awaitable)};
	}
};


template <typename AllowedErrorTypeList>
struct TCatchAdaptor : TAwaitableAdaptorBase<TCatchAdaptor<AllowedErrorTypeList>>
{
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TCatchAdaptor)
	{
		return TCatchAwaitable<AwaitableType, AllowedErrorTypeList>{Forward<AwaitableType>(Awaitable)};
	}
};


namespace Awaitables
{
	inline FCatchAllAdaptor CatchAll()
	{
		return {};
	}

	template <typename... ErrorTypesToCatch>
	TCatchAdaptor<TTypeList<ErrorTypesToCatch...>> Catch()
	{
		return {};
	}
}


using FWeakCoroutine = TWeakCoroutine<void>;
