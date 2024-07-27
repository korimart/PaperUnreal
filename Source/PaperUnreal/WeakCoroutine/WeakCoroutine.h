// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"
#include "AwaitableWrappers.h"
#include "CancellableFuture.h"
#include "MinimalCoroutine.h"
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
		return NewError<UWeakCoroutineError>(TEXT("AbortIfInvalid로 등록된 오브젝트가 Valid 하지 않음"));
	}
};


namespace WeakCoroutine_Private
{
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
		bool await_suspend(HandleType&& Handle, std::source_location SL = std::source_location::current())
		{
			SourceLocation = SL;

			if (Awaitable.await_ready())
			{
				if (ResumeInnerAndCheckValidity())
				{
					return false;
				}

				LogErrors();
				Handle.destroy();
				return true;
			}

			struct FWeakCheckingHandle
			{
				TWeakAbortingAwaitable* This;
				std::decay_t<HandleType> Handle;

				void resume() const
				{
					if (This->ResumeInnerAndCheckValidity())
					{
						Handle.resume();
						return;
					}

					This->LogErrors();
					Handle.destroy();
				}
			};

			Awaitable.await_suspend(FWeakCheckingHandle{this, Forward<HandleType>(Handle)});
			return true;
		}

		TFailableResult<ResultType> await_resume()
		{
			return MoveTemp(*Result);
		}

	private:
		AwaitableType Awaitable;
		TOptional<TFailableResult<ResultType>> Result;
		std::source_location SourceLocation;

		bool ResumeInnerAndCheckValidity()
		{
			Result = Awaitable.await_resume();
			for (UFailableResultError* Each : Result->GetErrors())
			{
				if (Each->IsA<UWeakCoroutineError>())
				{
					return false;
				}
			}
			return true;
		}

		void LogErrors() const
		{
			UE_LOG(LogTemp, Warning, TEXT("\n코루틴을 종료합니다.\n파일: %hs\n함수: %hs\n라인: %d\n이유:"),
				SourceLocation.file_name(), SourceLocation.function_name(), SourceLocation.line());
			Result->LogErrors();
		}
	};


	template <typename InAwaitableType, typename PromiseType>
	class TWeakAwaitable
	{
	public:
		using AwaitableType = typename TEnsureErrorReporting<InAwaitableType>::Type;
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
		auto await_suspend(HandleType&& Handle)
		{
			return Awaitable.await_suspend(Forward<HandleType>(Handle));
		}

		TFailableResult<ResultType> await_resume()
		{
			TFailableResult<ResultType> Ret = Awaitable.await_resume();

			if (!Handle.promise().IsValid())
			{
				Ret.AddError(UWeakCoroutineError::InvalidCoroutine());
			}

			return Ret;
		}

	private:
		std::coroutine_handle<PromiseType> Handle;
		AwaitableType Awaitable;
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

	TWeakCoroutine(std::coroutine_handle<promise_type> InHandle, const TSharedRef<bool>& bFinished)
		: Handle(InHandle), bCoroutineFinished(bFinished)
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
		if (!*bCoroutineFinished)
		{
			Handle.promise().Abort();
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
	auto await_transform(AnyType&& Any)
	{
		return await_transform(operator co_await(Forward<AnyType>(Any)));
	}

	// TODO AsAbrotPtrReturning이 가장 바깥이 됨에 따라 source location이 올바르지 않게 되었음

	template <CAwaitable AwaitableType>
	auto await_transform(WeakCoroutine_Private::TWithErrorAwaitable<AwaitableType>&& Awaitable)
	{
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
		return AsAbortPtrReturning(WeakCoroutine_Private::TWeakAbortingAwaitable{WeakCoroutine_Private::TWeakAwaitable{Handle, MoveTemp(Awaitable.Inner())}});
	}

	template <CAwaitable AwaitableType>
	auto await_transform(AwaitableType&& Awaitable)
	{
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));
		return AsAbortPtrReturning(TErrorRemovedAwaitable{WeakCoroutine_Private::TWeakAwaitable{Handle, Forward<AwaitableType>(Awaitable)}});
	}

	void Abort()
	{
		*bCoroutineFinished = true;
	}

	bool IsValid() const
	{
		return !*bCoroutineFinished && Algo::AllOf(WeakList, [](const auto& Each) { return Each.IsValid(); });
	}

	template <CUObjectWrapper U>
	void AddToWeakList(const U& Weak)
	{
		WeakList.Emplace(TUObjectWrapperTypeTraits<U>::GetUObject(Weak));
	}

	template <typename U>
	void RemoveFromWeakList(const U& Weak)
	{
		WeakList.Remove(TUObjectWrapperTypeTraits<U>::GetUObject(Weak));
	}

protected:
	friend class TWeakCoroutine<T>;

	TArray<TWeakObjectPtr<const UObject>> WeakList;
	TUniquePtr<TWeakCoroutineContext<T>> Context;
	TUniquePtr<TUniqueFunction<ReturnObjectType(TWeakCoroutineContext<T>&)>> Captures;
	TOptional<TCancellablePromise<T>> Promise;
	TSharedRef<bool> bCoroutineFinished = MakeShared<bool>(false);

	template <CErrorReportingAwaitable AwaitableType>
	decltype(auto) AsAbortPtrReturning(AwaitableType&& Awaitable);

	template <CNonErrorReportingAwaitable AwaitableType>
	decltype(auto) AsAbortPtrReturning(AwaitableType&& Awaitable);
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

private:
	T* Ptr;
	TFunction<void(T*)> NoAbort;
};


template <typename Derived, typename T>
template <CErrorReportingAwaitable AwaitableType>
decltype(auto) TWeakCoroutinePromiseTypeBase<Derived, T>::AsAbortPtrReturning(AwaitableType&& Awaitable)
{
	using ResultType = typename std::decay_t<AwaitableType>::ResultType;

	if constexpr (CUObjectWrapper<ResultType>)
	{
		using AbortPtrType = TAbortPtr<std::remove_pointer_t<typename TUObjectWrapperTypeTraits<ResultType>::RawPtrType>>;
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));

		return TTransformingAwaitable
		{
			Forward<AwaitableType>(Awaitable),
			[Handle](TFailableResult<ResultType>&& Result) -> TFailableResult<AbortPtrType>
			{
				if (Result.Succeeded())
				{
					return {InPlace, Handle, TUObjectWrapperTypeTraits<ResultType>::GetRaw(Result.GetResult())};
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

	if constexpr (CUObjectWrapper<ResultType>)
	{
		using AbortPtrType = TAbortPtr<std::remove_pointer_t<typename TUObjectWrapperTypeTraits<ResultType>::RawPtrType>>;
		auto Handle = std::coroutine_handle<Derived>::from_promise(*static_cast<Derived*>(this));

		return TTransformingAwaitable
		{
			Forward<AwaitableType>(Awaitable),
			[Handle](ResultType&& Result) -> AbortPtrType
			{
				return {Handle, MoveTemp(Result)};
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


// TODO
template <typename... AllowedErrorTypes, typename MaybeAwaitableType>
auto WithError(MaybeAwaitableType&& MaybeAwaitable)
{
	return WeakCoroutine_Private::TWithErrorAwaitable{AwaitableOrIdentity(Forward<MaybeAwaitableType>(MaybeAwaitable))};
}


using FWeakCoroutine = TWeakCoroutine<void>;
using FWeakCoroutineContext = TWeakCoroutineContext<void>;
