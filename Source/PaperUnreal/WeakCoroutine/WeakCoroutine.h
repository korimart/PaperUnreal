// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AbortablePromise.h"
#include "AwaitablePromise.h"
#include "CancellableFuture.h"
#include "LoggingPromise.h"
#include "MiscAwaitables.h"
#include "TypeTraits.h"
#include "WeakPromise.h"
#include "WeakCoroutine.generated.h"


#if _MSC_VER 
	static_assert(_MSC_VER >= 1780, "Weak Coroutine이 작성된 MSVC 버전이 아닙니다. 그래도 진행하려면 이 라인을 지우고 컴파일 하세요.");
#endif


UCLASS()
class UWeakCoroutineError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UWeakCoroutineError* InvalidCoroutine()
	{
		return NewError<UWeakCoroutineError>(TEXT("WeakList에 등록된 오브젝트가 Valid 하지 않음"));
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


template <typename>
class TWeakCoroutinePromiseType;


template <typename T>
class TWeakCoroutine
	: public TAwaitableCoroutine<TWeakCoroutine<T>, T>
	  , public TAbortableCoroutine<TWeakCoroutine<T>>
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: TAwaitableCoroutine<TWeakCoroutine<T>, T>(InHandle)
		  , Handle(InHandle)
		  , PromiseLife(Handle.promise().PromiseLife)
	{
	}

	// TODO LambdaCapturableCoroutine으로 이동
	void Init(TUniquePtr<TUniqueFunction<TWeakCoroutine()>> Captures)
	{
		Handle.promise().Captures = MoveTemp(Captures);
		Handle.resume();
	}

	void AddToWeakList(const UObject* Object)
	{
		if (PromiseLife.IsValid())
		{
			Handle.promise().AddToWeakList(Object);
		}
	}

private:
	friend class TAwaitableCoroutine<TWeakCoroutine, T>;
	friend class TAbortableCoroutine<TWeakCoroutine>;

	std::coroutine_handle<promise_type> Handle;
	TWeakPtr<std::monostate> PromiseLife;

	void OnAwaitAbort()
	{
		this->Abort();
	}
};


template <typename T>
class TWeakCoroutinePromiseType : public FLoggingPromise
                                  , public TAbortablePromise<TWeakCoroutinePromiseType<T>>
                                  , public TWeakPromise<TWeakCoroutinePromiseType<T>>
                                  , public TAwaitablePromise<T>
{
public:
	TWeakCoroutinePromiseType() = default;

	TWeakCoroutinePromiseType(const std::invocable auto& Lambda)
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

	template <CAwaitableConvertible AnyType>
	auto await_transform(AnyType&& Any)
	{
		return await_transform(operator co_await(Forward<AnyType>(Any)));
	}

	template <CUObjectUnsafeWrapper UnsafeWrapperType>
	auto await_transform(const UnsafeWrapperType& Wrapper)
	{
		return Awaitables::AsAwaitable(Wrapper) | Awaitables::ReturnAsAbortPtr(*this);
	}

	template <typename WithErrorAwaitableType>
		requires TIsInstantiationOf_V<WithErrorAwaitableType, TCatchAwaitable>
	auto await_transform(WithErrorAwaitableType&& Awaitable)
	{
		using AllowedErrorTypeList = typename std::decay_t<WithErrorAwaitableType>::AllowedErrorTypeList;

		return Forward<WithErrorAwaitableType>(Awaitable).Awaitable
			| Awaitables::DestroyIfAbortRequested()
			| Awaitables::DestroyIfInvalidPromise()
			| Awaitables::DestroyIfErrorNotIn<AllowedErrorTypeList>()
			| Awaitables::ReturnAsAbortPtr(*this)
			| Awaitables::CaptureSourceLocation();
	}

	template <CAwaitable AwaitableType>
		requires !TIsInstantiationOf_V<AwaitableType, TCatchAwaitable>
	auto await_transform(AwaitableType&& Awaitable)
	{
		return Forward<AwaitableType>(Awaitable)
			| Awaitables::DestroyIfAbortRequested()
			| Awaitables::DestroyIfInvalidPromise()
			| Awaitables::DestroyIfError()
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
		// TODO 만약에 프로그램이 종료 중인 경우
		// 엔진이 종료되면서 이 destructor가 호출되는데 이 때 엔진 종료 중이기 때문에 NewObject가 실패함
		// 그래서 일단 이 경우에는 에러를 주지 않도록 막음 근데 문제는 이러면 이 에러에 의존하는
		// 특정 리소스의 clean up이 제대로 이루어지지 않을 수 있음 현재는 그런 경우는 없지만 추후 수정 필요
		if (!IsEngineExitRequested())
		{
			AddError(UWeakCoroutineError::ExplicitAbort());
		}
	}

	void OnAbortByInvalidity()
	{
		// TODO 만약에 프로그램이 종료 중인 경우
		// 엔진이 종료되면서 이 destructor가 호출되는데 이 때 엔진 종료 중이기 때문에 NewObject가 실패함
		// 그래서 일단 이 경우에는 에러를 주지 않도록 막음 근데 문제는 이러면 이 에러에 의존하는
		// 특정 리소스의 clean up이 제대로 이루어지지 않을 수 있음 현재는 그런 경우는 없지만 추후 수정 필요
		if (!IsEngineExitRequested())
		{
			AddError(UWeakCoroutineError::InvalidCoroutine());
		}
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


template <typename AwaitableType>
	requires CAwaitable<AwaitableType> || CAwaitableConvertible<AwaitableType>
auto RunWeakCoroutine(const UObject* Lifetime, AwaitableType&& Awaitable)
{
	return RunWeakCoroutine(Lifetime,
		[Awaitable = AwaitableType{Forward<AwaitableType>(Awaitable)}]() mutable -> TWeakCoroutine<void>
		{
			co_await Awaitable;
		});
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
