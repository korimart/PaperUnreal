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
	friend class TWeakCoroutine<T>;
	std::coroutine_handle<TWeakCoroutinePromiseType<T>> Handle;
};


template <typename T>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;
	using ContextType = TWeakCoroutineContext<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle), AbortablePromiseHandle(Handle.promise().GetAbortableHandle())
	{
	}

	void Init(
		TUniquePtr<TUniqueFunction<TWeakCoroutine(TWeakCoroutineContext<T>&)>> Captures,
		TUniquePtr<TWeakCoroutineContext<T>> Context)
	{
		Handle.promise().Captures = MoveTemp(Captures);
		Handle.promise().Context = MoveTemp(Context);
		Handle.promise().Context->Handle = Handle;

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
		AbortablePromiseHandle.Abort();
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
	FAbortablePromiseHandle AbortablePromiseHandle;
	TOptional<TCancellableFuture<T>> CoroutineRet;
};


template <typename Derived, typename T>
class TWeakCoroutinePromiseTypeBase : public FLoggingPromise
                                      , public TAbortablePromise<TWeakCoroutinePromiseTypeBase<Derived, T>>
                                      , public TWeakPromise<TWeakCoroutinePromiseTypeBase<Derived, T>>
{
public:
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

		return TSourceLocationAwaitable{
			AsAbortPtrReturning(
				FilterErrors<AllowedErrorTypeList>(
					TWeakAwaitable{*this, MoveTemp(Awaitable.Inner())}))
		};
	}

	template <CAwaitable AwaitableType>
	auto await_transform(AwaitableType&& Awaitable)
	{
		return TSourceLocationAwaitable{
			AsAbortPtrReturning(
				RemoveErrors(
					TWeakAwaitable{*this, Forward<AwaitableType>(Awaitable)}))
		};
	}

protected:
	friend class TWeakCoroutine<T>;

	TUniquePtr<TWeakCoroutineContext<T>> Context;
	TUniquePtr<TUniqueFunction<ReturnObjectType(TWeakCoroutineContext<T>&)>> Captures;
	TOptional<TCancellablePromise<T>> Promise;

	template <CErrorReportingAwaitable AwaitableType>
	decltype(auto) AsAbortPtrReturning(AwaitableType&& Awaitable);

	template <CNonErrorReportingAwaitable AwaitableType>
	decltype(auto) AsAbortPtrReturning(AwaitableType&& Awaitable);

	template <typename AllowedErrorTypeList, typename AwaitableType>
	decltype(auto) FilterErrors(AwaitableType&& Awaitable)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TErrorFilteringAwaitable<AwaitableType, AllowedErrorTypeList>{Forward<AwaitableType>(Awaitable)};
		}
	}

	template <typename AwaitableType>
	decltype(auto) RemoveErrors(AwaitableType&& Awaitable)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TErrorRemovingAwaitable{Forward<AwaitableType>(Awaitable)};
		}
	}

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


template <typename T>
class TAbortPtr
{
public:
	/**
	 * 여기서 핸들을 받는 것이 안전한 이유는 다음과 같음
	 * TAbortPtr는 Weak Coroutine의 코루틴 프레임 내에서 co_await에 의해서만 생성될 수 있음
	 * 그리고 copy 불가능 move 불가능이기 때문에 코루틴 프레임 내에서 평생을 살아감
	 * 즉 생성과 파괴가 코루틴에 의해서 일어나고 그 안에서만 살기 떄문에 Handle이 TAbortPtr보다 더 오래 살음
	 * 
	 * @param Handle 
	 * @param Pointer 
	 */
	TAbortPtr(auto Handle, T* Pointer)
		: Ptr(Pointer)
	{
		if (Ptr)
		{
			Handle.promise().AddToWeakList(Ptr);

			NoAbort = [Handle](T* Ptr)
			{
				Handle.promise().RemoveFromWeakList(Ptr);
			};
		}
	}

	~TAbortPtr()
	{
		if (Ptr)
		{
			NoAbort(Ptr);
		}
	}

	// 절대 허용해서는 안 됨 constructor의 주석 참고
	// copy 또는 move를 허용하면 Handle이 코루틴 프레임 바깥으로 탈출할 수 있음
	TAbortPtr(const TAbortPtr&) = delete;
	TAbortPtr& operator=(const TAbortPtr&) = delete;
	TAbortPtr(TAbortPtr&&) = delete;
	TAbortPtr& operator=(TAbortPtr&) = delete;

	/**
	 * lvalue일 때만 허용하지 않으면 Weak Coroutine 내에서 co_await으로 TAbortPtr를 받을 때
	 * T*로 실수로 받아서 invalid 시 abort가 곧바로 해제될 수 있음
	 * 한 번은 일단 TAbortPtr로 받은 다음에 함수 호출 등에는 implicit conversion으로 그냥 넘길 수 있도록
	 * lvalue일 때는 허용하되 rvalue일 때는 허용하지 않음
	 */
	operator T*() & { return Ptr; }

	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }

	T* Get() const { return Ptr; }

private:
	T* Ptr;
	TFunction<void(T*)> NoAbort;
};


template <typename Derived, typename T>
template <CErrorReportingAwaitable AwaitableType>
decltype(auto) TWeakCoroutinePromiseTypeBase<Derived, T>::AsAbortPtrReturning(AwaitableType&& Awaitable)
{
	using ResultType = typename std::decay_t<AwaitableType>::ResultType;

	if constexpr (CUObjectUnsafeWrapper<ResultType>)
	{
		using AbortPtrType = TAbortPtr<std::remove_pointer_t<typename TUObjectUnsafeWrapperTypeTraits<ResultType>::RawPtrType>>;
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));

		return TTransformingAwaitable
		{
			Forward<AwaitableType>(Awaitable),
			[Handle](TFailableResult<ResultType>&& Result) -> TFailableResult<AbortPtrType>
			{
				if (Result.Succeeded())
				{
					return {InPlace, Handle, TUObjectUnsafeWrapperTypeTraits<ResultType>::GetRaw(Result.GetResult())};
				}

				return Result.GetErrors();
			},
		};
	}
	else
	{
		return Forward<AwaitableType>(Awaitable);
	}
}

template <typename Derived, typename T>
template <CNonErrorReportingAwaitable AwaitableType>
decltype(auto) TWeakCoroutinePromiseTypeBase<Derived, T>::AsAbortPtrReturning(AwaitableType&& Awaitable)
{
	using ResultType = decltype(Awaitable.await_resume());

	if constexpr (CUObjectUnsafeWrapper<ResultType>)
	{
		using AbortPtrType = TAbortPtr<std::remove_pointer_t<typename TUObjectUnsafeWrapperTypeTraits<ResultType>::RawPtrType>>;
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));

		return TTransformingAwaitable
		{
			Forward<AwaitableType>(Awaitable),
			[Handle](ResultType&& Result) -> AbortPtrType
			{
				return {Handle, TUObjectUnsafeWrapperTypeTraits<ResultType>::GetRaw(MoveTemp(Result))};
			},
		};
	}
	else
	{
		return Forward<AwaitableType>(Awaitable);
	}
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


template <typename... AllowedErrorTypes, typename MaybeAwaitableType>
auto WithError(MaybeAwaitableType&& MaybeAwaitable)
{
	using namespace WeakCoroutine_Private;

	using AllowedErrorTypeList = std::conditional_t
	<
		sizeof...(AllowedErrorTypes) == 0,
		FAllowAll,
		TTypeList<AllowedErrorTypes...>
	>;

	auto Awaitable = AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable));
	return TWithErrorAwaitable<decltype(Awaitable), AllowedErrorTypeList>{MoveTemp(Awaitable)};
}


using FWeakCoroutine = TWeakCoroutine<void>;
using FWeakCoroutineContext = TWeakCoroutineContext<void>;
