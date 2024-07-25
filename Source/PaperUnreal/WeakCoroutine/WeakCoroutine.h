// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "CancellableFuture.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "WeakCoroutine.generated.h"


UCLASS()
class UWeakCoroutineError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UWeakCoroutineError* InvalidCoroutine()
	{
		return NewError<UWeakCoroutineError>(TEXT("InvalidCoroutine"));
	}
};


namespace WeakCoroutineDetails
{
	template <typename T, typename WeakListContainerType>
	concept CWeakListAddable = requires(T Arg, WeakListContainerType Container)
	{
		Container.AddToWeakList(Arg);
	};

	template <typename AwaitableType, bool>
	struct TAlwaysAssumingAwaitableIfTrue
	{
		using Type = void;
	};

	template <typename AwaitableType>
	struct TAlwaysAssumingAwaitableIfTrue<AwaitableType, true>
	{
		using Type = TAlwaysResumingAwaitable<AwaitableType>;
	};

	template <typename AwaitableType>
	class TWithErrorAwaitable : public TIdentityAwaitable<AwaitableType>
	{
	public:
		using TIdentityAwaitable<AwaitableType>::TIdentityAwaitable;
	};

	template <CErrorReportingAwaitable AwaitableType>
	class TWeakAbortingAwaitable
	{
	public:
		using ResultType = typename AwaitableType::ResultType;
		
		TWeakAbortingAwaitable(AwaitableType&& Inner)
			: Awaitable(MoveTemp(Inner))
		{
		}

		bool await_ready() const
		{
			return false;
		}

		template <typename HandleType>
		bool await_suspend(HandleType&& Handle)
		{
			if (Awaitable.await_ready())
			{
				Result = Awaitable.await_resume();
				for (UFailableResultError* Each : Result->GetErrors())
				{
					if (Each->IsA<UWeakCoroutineError>())
					{
						Handle.destroy();
						return true;
					}
				}
				return false;
			}
			
			Awaitable.await_suspend(Forward<HandleType>(Handle));
			return true;
		}

		TFailableResult<ResultType> await_resume()
		{
			return Awaitable.await_resume();
		}

	private:
		AwaitableType Awaitable;
		TOptional<TFailableResult<ResultType>> Result;
	};
}


template <typename>
class TWeakCoroutineContext;

template <typename>
class TWeakCoroutinePromiseType;


template <typename T>
class TWeakCoroutine
{
public:
	using promise_type = TWeakCoroutinePromiseType<T>;
	using ContextType = TWeakCoroutineContext<T>;

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle, const TSharedRef<bool>& bFinished)
		: Handle(InHandle), bCoroutineFinished(bFinished)
	{
	}

	void Init(
		TUniquePtr<TUniqueFunction<TWeakCoroutine(TWeakCoroutineContext<T>&)>> Captures,
		TUniquePtr<TWeakCoroutineContext<T>> Context);

	void Resume();

	void Abort();

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
	TSharedRef<bool> bCoroutineFinished;
	TOptional<TCancellableFuture<T>> CoroutineRet;
};


template <typename Derived, typename T>
class TWeakCoroutinePromiseTypeBase
{
public:
	using ReturnObjectType = TWeakCoroutine<T>;

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
	auto await_transform(AnyType&& Any) { return await_transform(operator co_await(Forward<AnyType>(Any))); }

	template <CAwaitable AwaitableType>
	auto await_transform(WeakCoroutineDetails::TWithErrorAwaitable<AwaitableType>&& Awaitable);

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

	template <typename U>
	void AddToWeakList(TScriptInterface<U> Interface)
	{
		AddToWeakList(Interface.GetObject());
	}

	template <typename U>
	void AddToWeakList(const TSharedPtr<U>& Object)
	{
		WeakList.Add([Weak = TWeakPtr<U>{Object}]() { return Weak.IsValid(); });
	}

	template <typename U>
	void AddToWeakList(const TSharedRef<U>& Object)
	{
		WeakList.Add([Weak = TWeakPtr<U>{Object}]() { return Weak.IsValid(); });
	}

	template <typename U>
	void AddToWeakList(const TWeakPtr<U>& Object)
	{
		WeakList.Add([Weak = Object]() { return Weak.IsValid(); });
	}

	template <typename U>
	void AddToWeakList(const TWeakObjectPtr<U>& Object)
	{
		WeakList.Add([Weak = Object]() { return Weak.IsValid(); });
	}

protected:
	friend class TWeakCoroutine<T>;

	TArray<TFunction<bool()>> WeakList;
	TUniquePtr<TWeakCoroutineContext<T>> Context;
	TUniquePtr<TUniqueFunction<ReturnObjectType(TWeakCoroutineContext<T>&)>> Captures;
	TOptional<TCancellablePromise<T>> Promise;
	TSharedRef<bool> bCoroutineFinished = MakeShared<bool>(false);
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
void TWeakCoroutine<T>::Init(
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

template <typename T>
void TWeakCoroutine<T>::Resume()
{
	Handle.resume();
}

template <typename T>
void TWeakCoroutine<T>::Abort()
{
	if (!*bCoroutineFinished)
	{
		Handle.promise().Abort();
	}
}


template <typename InAwaitableType, typename PromiseType>
class TWeakAwaitable
{
public:
	using AwaitableType = std::conditional_t<
		CErrorReportingAwaitable<InAwaitableType>,
		InAwaitableType,
		// 이거 TAlwaysAssumingAwaitable의 concept가 std::conditional_t의 결과와 관계없이 evaluate 되기 때문에 늦추려면 이렇게 해야됨
		// TODO 더 간결한 방법이 없는지 조사해본다
		typename WeakCoroutineDetails::TAlwaysAssumingAwaitableIfTrue<InAwaitableType, !CErrorReportingAwaitable<InAwaitableType>>::Type>;

	using ResultType = typename AwaitableType::ResultType;

	/**
	 * constructor에서 Handle을 받는 것이 안전한 이유는 이 클래스의 사용을 await_transform 내부로만 제한하기 때문임
	 * await_transform이 호출되었다는 것은 co_await 되었다는 뜻이고 그건 이 인스턴스가 Handle이 가리키는 coroutine frame 안에 존재한다는 것을 의미함
	 * 즉 await_transform에서 생성하는 이상 이 awaitable의 생명주기는 handle이 가리키는 promise_type을 벗어나지 않음
	 * 주의 : Handle을 사용해서 resume이나 destroy를 해서는 안 됨 다른 awaitable이 resume/destroy에 대한 권리를 가지고 있을 수 있음
	 * 만약 그런 처리가 필요한 경우 나에게 resume/destroy 권한이 있을 때(await_suspend로 핸들이 들어와 await_resume이 호출되기 전까지)만 가능
	 * 그 이외에는 반드시 read-only로만 사용해야 됨
	 * 
	 * @param InHandle 위에서 설명한 핸들
	 * @param InAwaitable Inner Awaitable
	 */
	TWeakAwaitable(std::coroutine_handle<PromiseType> InHandle, InAwaitableType&& InAwaitable)
		: Handle(InHandle), Awaitable(MoveTemp(InAwaitable))
	{
		static_assert(CErrorReportingAwaitable<TWeakAwaitable>);
	}

	bool await_ready() const
	{
		return Awaitable.await_ready();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		Awaitable.await_suspend(Forward<HandleType>(Handle));
	}

	TFailableResult<ResultType> await_resume()
	{
		TFailableResult<ResultType> Ret = Awaitable.await_resume();
		
		if (!Handle.promise().IsValid())
		{
			Ret.AddError(UWeakCoroutineError::InvalidCoroutine());
		}

		if constexpr (WeakCoroutineDetails::CWeakListAddable<ResultType, PromiseType>)
		{
			if (Ret.Succeeded())
			{
				Handle.promise().AddToWeakList(Ret.GetResult());
			}
		}

		return Ret;
	}

private:
	std::coroutine_handle<PromiseType> Handle;
	AwaitableType Awaitable;
};


template <typename Derived, typename T>
template <CAwaitable AwaitableType>
auto TWeakCoroutinePromiseTypeBase<Derived, T>::await_transform(WeakCoroutineDetails::TWithErrorAwaitable<AwaitableType>&& Awaitable)
{
	auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
	return WeakCoroutineDetails::TWeakAbortingAwaitable{TWeakAwaitable{Handle, MoveTemp(Awaitable.Inner())}};
}

template <typename Derived, typename T>
template <CAwaitable AwaitableType>
auto TWeakCoroutinePromiseTypeBase<Derived, T>::await_transform(AwaitableType&& Awaitable)
{
	auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
	return TErrorRemovedAwaitable{TWeakAwaitable{Handle, Forward<AwaitableType>(Awaitable)}};
}


template <typename MaybeAwaitableType>
auto WithError(MaybeAwaitableType&& MaybeAwaitable)
{
	return WeakCoroutineDetails::TWithErrorAwaitable{AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable))};
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
