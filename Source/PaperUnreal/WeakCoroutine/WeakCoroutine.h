// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AbortablePromise.h"
#include "AwaitablePromise.h"
#include "TransformAwaitable.h"
#include "CancellableFuture.h"
#include "LoggingPromise.h"
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
struct TCatchAwaitable
{
	using AllowedErrorTypeList = InAllowedErrorTypeList;

	AwaitableType Awaitable;
};


struct FSuspendConditional {
	bool bSuspend = false;
    constexpr bool await_ready() const noexcept { return !bSuspend; }
    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};


template <typename>
class TWeakCoroutinePromiseType;


template <typename T>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle), PromiseLife(Handle.promise().PromiseLife), PromiseReturn(Handle.promise().GetFuture())
	{
	}

	void Init(TUniquePtr<TUniqueFunction<TWeakCoroutine()>> Captures)
	{
		Handle.promise().Captures = MoveTemp(Captures);
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
	TWeakCoroutinePromiseType() = default;

	// TODO static assert functor type
	TWeakCoroutinePromiseType(const auto& Lambda)
		: bInitRequired(true)
	{
	}

	template <typename... ArgTypes>
	TWeakCoroutinePromiseType(const UObject& Object, ArgTypes&&... Args)
		: TWeakPromise<TWeakCoroutinePromiseType>(Object, Forward<ArgTypes>(Args)...)
	{
	}
	
	TWeakCoroutine<T> get_return_object()
	{
		return std::coroutine_handle<TWeakCoroutinePromiseType>::from_promise(*this);
	}

	FSuspendConditional initial_suspend() const
	{
		return {bInitRequired};
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

		return Forward<WithErrorAwaitableType>(Awaitable).Awaitable
			| Awaitables::AbortIfRequested()
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
			| Awaitables::AbortIfRequested()
			| Awaitables::AbortIfInvalidPromise()
			| Awaitables::AbortIfError()
			| Awaitables::ReturnAsAbortPtr(*this)
			| Awaitables::CaptureSourceLocation();
	}

private:
	friend class TWeakCoroutine<T>;
	friend struct TAbortablePromise<TWeakCoroutinePromiseType>;
	friend struct TWeakPromise<TWeakCoroutinePromiseType>;

	bool bInitRequired = false;
	TUniquePtr<TUniqueFunction<TWeakCoroutine<T>()>> Captures;
	TSharedRef<std::monostate> PromiseLife = MakeShared<std::monostate>();

	void OnAbortRequested()
	{
		AddError(UWeakCoroutineError::ExplicitAbort());
	}

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
	return WeakCoroutine;
}


template <typename FuncType>
auto RunWeakCoroutine(const UObject* Lifetime, FuncType&& Func)
{
	using CoroutineType = typename TGetReturnType<FuncType>::Type;

	// TUniqueFunction 구현 보면 32바이트 이하는 힙에 할당하지 않고 inline 할당하기 때문에 항상 UniquePtr 써줘야 함
	auto LambdaCaptures = MakeUnique<TUniqueFunction<CoroutineType()>>(Forward<FuncType>(Func));

	CoroutineType WeakCoroutine = (*LambdaCaptures)();
	WeakCoroutine.AddToWeakList(Lifetime);
	WeakCoroutine.Init(MoveTemp(LambdaCaptures));
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
