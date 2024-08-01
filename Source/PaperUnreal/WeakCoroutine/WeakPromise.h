// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"


template <typename Derived>
struct TWeakPromise
{
	bool IsValid() const
	{
		return Algo::AllOf(WeakList, [](const auto& Each) { return Each.IsValid(); });
	}

	template <CUObjectUnsafeWrapper WrapperType>
	void AddToWeakList(const WrapperType& Wrapper)
	{
		WeakList.Emplace(TUObjectUnsafeWrapperTypeTraits<WrapperType>::GetUObject(Wrapper));
	}

	template <CUObjectUnsafeWrapper WrapperType>
	void RemoveFromWeakList(const WrapperType& Wrapper)
	{
		WeakList.Remove(TUObjectUnsafeWrapperTypeTraits<WrapperType>::GetUObject(Wrapper));
	}

private:
	template <typename>
	friend class TWeakAwaitable;

	TArray<TWeakObjectPtr<const UObject>> WeakList;

	void OnWeakAwaitableNoResume()
	{
		static_cast<Derived*>(this)->OnAbortByInvalidity();
	}
};


template <typename InnerAwaitableType>
class TWeakAwaitable
	: public TConditionalResumeAwaitable<TWeakAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TWeakAwaitable, InnerAwaitableType>;
	using ReturnType = typename Super::ReturnType;

	template <typename AwaitableType>
	TWeakAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
	}

	bool ShouldResume(const auto& WeakPromiseHandle, const auto&) const
	{
		if (!WeakPromiseHandle.promise().IsValid())
		{
			WeakPromiseHandle.promise().OnWeakAwaitableNoResume();
			return false;
		}

		return true;
	}
};

template <typename AwaitableType>
TWeakAwaitable(AwaitableType&& Awaitable) -> TWeakAwaitable<AwaitableType>;


struct FAbortIfInvalidPromiseAdaptor : TAwaitableAdaptorBase<FAbortIfInvalidPromiseAdaptor>
{
	template <typename AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, FAbortIfInvalidPromiseAdaptor)
	{
		return TWeakAwaitable{Forward<AwaitableType>(Awaitable)};
	}
};


struct FAddToWeakListAwaitable
{
	FAddToWeakListAwaitable(const TWeakObjectPtr<const UObject>& InObject)
		: Object(InObject)
	{
		check(!Object.IsExplicitlyNull());
	}

	~FAddToWeakListAwaitable()
	{
		check(Object.IsExplicitlyNull());
	}

	FAddToWeakListAwaitable(const FAddToWeakListAwaitable&) = delete;
	FAddToWeakListAwaitable& operator=(const FAddToWeakListAwaitable&) = delete;

	FAddToWeakListAwaitable(FAddToWeakListAwaitable&& Other) noexcept
		: Object(MoveTemp(Other.Object))
	{
		Other.Object = nullptr;
	}

	FAddToWeakListAwaitable& operator=(FAddToWeakListAwaitable&& Other) noexcept
	{
		Object = MoveTemp(Other.Object);
		Other.Object = nullptr;
		return *this;
	}

	bool await_ready() const
	{
		return false;
	}

	bool await_suspend(const auto& Handle)
	{
		Handle.promise().AddToWeakList(Object.Get());
		Object = nullptr;
		return false;
	}

	std::monostate await_resume()
	{
		return {};
	}

private:
	TWeakObjectPtr<const UObject> Object;
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


template <typename WeakPromiseType>
struct TReturnAsAbortPtrAdaptor : TAwaitableAdaptorBase<TReturnAsAbortPtrAdaptor<WeakPromiseType>>
{
	std::coroutine_handle<WeakPromiseType> Handle;
	
	TReturnAsAbortPtrAdaptor(std::coroutine_handle<WeakPromiseType> InHandle)
		: Handle(InHandle)
	{
	}

	template <CErrorReportingAwaitable AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, const TReturnAsAbortPtrAdaptor& Adaptor)
	{
		using ResultType = typename std::decay_t<AwaitableType>::ResultType;

		if constexpr (!CUObjectUnsafeWrapper<ResultType>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			using ResultAsRawPtrType = typename TUObjectUnsafeWrapperTypeTraits<ResultType>::RawPtrType;
			using AbortPtrType = TAbortPtr<std::remove_pointer_t<ResultAsRawPtrType>>;

			// 아래 Handle 복사에 대한 이유는 TAbortPtr의 주석 참고

			return TTransformingAwaitable
			{
				Forward<AwaitableType>(Awaitable),
				[Handle = Adaptor.Handle](TFailableResult<ResultType>&& Result) -> TFailableResult<AbortPtrType>
				{
					if (Result.Succeeded())
					{
						return {InPlace, Handle, TUObjectUnsafeWrapperTypeTraits<ResultType>::GetRaw(Result.GetResult())};
					}

					return Result.GetErrors();
				},
			};
		}
	}

	template <CNonErrorReportingAwaitable AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, const TReturnAsAbortPtrAdaptor& Adaptor)
	{
		using ResultType = decltype(Awaitable.await_resume());

		if constexpr (!CUObjectUnsafeWrapper<ResultType>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			using ResultAsRawPtrType = typename TUObjectUnsafeWrapperTypeTraits<ResultType>::RawPtrType;
			using AbortPtrType = TAbortPtr<std::remove_pointer_t<ResultAsRawPtrType>>;

			// 아래 Handle 복사에 대한 이유는 TAbortPtr의 주석 참고

			return TTransformingAwaitable
			{
				Forward<AwaitableType>(Awaitable),
				[Handle = Adaptor.Handle](ResultType&& Result) -> AbortPtrType
				{
					return {Handle, TUObjectUnsafeWrapperTypeTraits<ResultType>::GetRaw(MoveTemp(Result))};
				},
			};
		}
	}
};


namespace Awaitables
{
	inline FAbortIfInvalidPromiseAdaptor AbortIfInvalidPromise()
	{
		return {};
	}


	template <typename WeakPromiseType>
	TReturnAsAbortPtrAdaptor<WeakPromiseType> ReturnAsAbortPtr(WeakPromiseType& Promise)
	{
		return {std::coroutine_handle<WeakPromiseType>::from_promise(Promise)};
	}
}


template <CUObjectUnsafeWrapper WrapperType>
FAddToWeakListAwaitable AddToWeakList(const WrapperType& Wrapper)
{
	return {TUObjectUnsafeWrapperTypeTraits<WrapperType>::GetUObject(Wrapper)};
}
