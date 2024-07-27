// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "TypeTraits.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/AssetManager.h"
#include "CancellableFuture.generated.h"


namespace FutureDetails
{
	template <typename FuncType>
	void BindOneOff(CDelegate auto& Delegate, FuncType&& Func)
	{
		TSharedRef<bool> Life = MakeShared<bool>(false);
		Delegate.BindSPLambda(Life, [Life = Life.ToSharedPtr(), Func = Forward<FuncType>(Func)]<typename... ArgTypes>(ArgTypes&&... Args) mutable
		{
			if (Life && !*Life)
			{
				Func(Forward<ArgTypes>(Args)...);
				*Life = true;
				Life = nullptr;
			}
		});
	}

	template <CMulticastDelegate MulticastDelegateType, typename FuncType>
	void BindOneOff(MulticastDelegateType& MulticastDelegate, FuncType&& Func)
	{
		typename MulticastDelegateType::FDelegate Delegate;
		BindOneOff(Delegate, Forward<FuncType>(Func));
		MulticastDelegate.Add(Delegate);
	}
}


UCLASS()
class UCancellableFutureError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UCancellableFutureError* PromiseNotFulfilled()
	{
		return NewError<UCancellableFutureError>(TEXT("PromiseNotFulfilled"));
	}
	
	static UCancellableFutureError* Cancelled()
	{
		return NewError<UCancellableFutureError>(TEXT("Cancelled"));
	}
};


template <typename T>
class TCancellableFutureState
{
	static constexpr bool bVoid = std::is_same_v<T, void>;

public:
	using ResultType = std::conditional_t<bVoid, std::monostate, T>;

	TCancellableFutureState() = default;
	TCancellableFutureState(const TCancellableFutureState&) = delete;
	TCancellableFutureState& operator=(const TCancellableFutureState&) = delete;

	void SetValue() requires bVoid
	{
		SetValue(std::monostate{});
	}

	template <typename U>
	void SetValue(U&& Value)
	{
		Ret.Emplace(TFailableResult<ResultType>{Forward<U>(Value)});
		
		if (Callback)
		{
			Callback(PeekValue());
		}
	}

	bool HasValue() const
	{
		return Ret.IsSet();
	}

	const TFailableResult<ResultType>& PeekValue() const
	{
		check(!bResultConsumed);
		return *Ret;
	}
	
	TFailableResult<ResultType> ConsumeValue() &&
	{
		check(!bResultConsumed);
		bResultConsumed = true;
		return MoveTemp(*Ret);
	}

	template <typename FuncType>
	void OnArrival(FuncType&& Func)
	{
		check(!bResultConsumed && !Callback);
		Callback = Forward<FuncType>(Func);
	}

private:
	bool bResultConsumed = false;
	TOptional<TFailableResult<ResultType>> Ret;
	TUniqueFunction<void(const TFailableResult<ResultType>&)> Callback;
};


template <typename T>
class TCancellableFuture
{
public:
	using StateType = TCancellableFutureState<T>;
	using ResultType = typename StateType::ResultType;

	TCancellableFuture() requires std::is_same_v<T, void>
		: State(MakeShared<StateType>())
	{
		State->SetValue();
	}

	TCancellableFuture(const TSharedRef<StateType>& InState)
		: State(InState)
	{
	}

	template <typename U>
	TCancellableFuture(U&& ReadyValue) requires !std::is_convertible_v<U, const TSharedRef<StateType>&>
		: State(MakeShared<StateType>())
	{
		State->SetValue(Forward<U>(ReadyValue));
	}

	TCancellableFuture(const TCancellableFuture&) = delete;
	TCancellableFuture& operator=(const TCancellableFuture&) = delete;

	TCancellableFuture(TCancellableFuture&&) = default;
	TCancellableFuture& operator=(TCancellableFuture&&) = default;

	bool IsConsumed() const
	{
		return !State;
	}

	bool IsReady() const
	{
		check(!IsConsumed());
		return State->HasValue();
	}

	template <typename FuncType>
	void Then(FuncType&& Func) const
	{
		check(!IsConsumed());

		if (State->HasValue())
		{
			Func(State->PeekValue());
		}
		else
		{
			State->OnArrival(Forward<FuncType>(Func));
		}
	}

	TFailableResult<ResultType> ConsumeValue() &&
	{
		check(IsReady());
		TFailableResult<ResultType> Ret = MoveTemp(*State).ConsumeValue();
		State = nullptr;
		return Ret;
	}
	
	const TFailableResult<ResultType>& PeekValue() const
	{
		check(IsReady());
		return State->PeekValue();
	}

private:
	TSharedPtr<StateType> State;
};


template <typename T>
class TCancellablePromise
{
	static constexpr bool bVoidResult = std::is_same_v<T, void>;

public:
	using StateType = TCancellableFutureState<T>;

	TCancellablePromise() = default;

	TCancellablePromise(const TCancellablePromise&) = delete;
	TCancellablePromise& operator=(const TCancellablePromise&) = delete;

	TCancellablePromise(TCancellablePromise&&) = default;
	TCancellablePromise& operator=(TCancellablePromise&&) = default;

	~TCancellablePromise()
	{
		if (State)
		{
			State->SetValue(UCancellableFutureError::PromiseNotFulfilled());
		}
	}

	bool IsSet() const
	{
		return !State;
	}

	void SetValue() requires bVoidResult
	{
		check(!IsSet());
		std::exchange(State, nullptr)->SetValue();
	}

	template <typename U>
	void SetValue(U&& Value)
	{
		check(!IsSet());
		std::exchange(State, nullptr)->SetValue(Forward<U>(Value));
	}

	void Cancel()
	{
		check(!IsSet());
		std::exchange(State, nullptr)->SetValue(UCancellableFutureError::Cancelled());
	}

	TCancellableFuture<T> GetFuture()
	{
		// 이미 값이 준비가 되어 있다면 Promise를 통하는 것이 아니라 Ready Future를 만들어야 됨
		check(!bMadeFuture && !IsSet());
		bMadeFuture = true;
		return {State.ToSharedRef()};
	}

private:
	bool bMadeFuture = false;
	TSharedPtr<StateType> State = MakeShared<StateType>();
};


template <typename T>
class TShareableCancellablePromise
{
public:
	static constexpr bool bVoidResult = std::is_same_v<T, void>;
	using PromiseType = TCancellablePromise<T>;

	TShareableCancellablePromise() = default;

	void SetValue() requires bVoidResult
	{
		check(!HasThisInstanceSet());

		if (!Promise->IsSet())
		{
			Promise->SetValue();
		}

		Promise = nullptr;
	}

	template <typename U>
	void SetValue(U&& Value)
	{
		check(!HasThisInstanceSet());

		if (!Promise->IsSet())
		{
			Promise->SetValue(Forward<U>(Value));
		}

		Promise = nullptr;
	}

	void Cancel()
	{
		check(!HasThisInstanceSet());

		if (!Promise->IsSet())
		{
			Promise->Cancel();
		}

		Promise = nullptr;
	}

	TCancellableFuture<T> GetFuture()
	{
		check(!HasThisInstanceSet());
		return Promise->GetFuture();
	}

	bool HasThisInstanceSet() const
	{
		return !Promise;
	}

private:
	TSharedPtr<PromiseType> Promise = MakeShared<PromiseType>();
};


template <typename T>
class TCancellableFutureAwaitable
{
public:
	using FutureType = TCancellableFuture<T>;
	using ResultType = typename FutureType::ResultType;

	TCancellableFutureAwaitable(FutureType&& InFuture)
		: Future(MoveTemp(InFuture))
	{
		static_assert(CErrorReportingAwaitable<TCancellableFutureAwaitable>);
	}

	TCancellableFutureAwaitable(const TCancellableFutureAwaitable&) = delete;
	TCancellableFutureAwaitable& operator=(const TCancellableFutureAwaitable&) = delete;

	TCancellableFutureAwaitable(TCancellableFutureAwaitable&& Other) noexcept = default;
	TCancellableFutureAwaitable& operator=(TCancellableFutureAwaitable&& Other) = delete;

	bool await_ready() const
	{
		// 이미 co_await을 끝낸 Future를 또 co_wait한 경우 여기 들어올 수 있음
		check(!Future.IsConsumed());
		return Future.IsReady();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		Future.Then([this, Handle = Forward<HandleType>(Handle)](auto&)
		{
			Handle.resume();
		});
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(Future).ConsumeValue();
	}

private:
	FutureType Future;
};


template <typename T>
TCancellableFutureAwaitable<T> operator co_await(TCancellableFuture<T>&& Future)
{
	return {MoveTemp(Future)};
}


template <typename T>
TTuple<TCancellablePromise<T>, TCancellableFuture<T>> MakePromise()
{
	TCancellablePromise<T> Ret;
	TCancellableFuture<T> Future = Ret.GetFuture();
	return MakeTuple(MoveTemp(Ret), MoveTemp(Future));
}


template <typename T>
TTuple<TShareableCancellablePromise<T>, TCancellableFuture<T>> MakeShareablePromise()
{
	TShareableCancellablePromise<T> Ret;
	return MakeTuple(Ret, Ret.GetFuture());
}


template <CDelegate DelegateType>
auto MakeFutureFromDelegate(DelegateType& Delegate)
{
	using ParamTypeTuple = typename TFuncParamTypesToTypeList<TTuple, typename DelegateType::TFuncType>::Type;
	constexpr int32 ParamCount = TTypeListCount_V<ParamTypeTuple>;

	if constexpr (ParamCount == 0)
	{
		return MakeFutureFromDelegate<void>(Delegate,
			[]() { return true; },
			[]() { return; });
	}
	else if constexpr (ParamCount == 1)
	{
		using ResultType = std::decay_t<typename TFirstInTypeList<ParamTypeTuple>::Type>;
		return MakeFutureFromDelegate<ResultType>(Delegate,
			[](auto) { return true; },
			[]<typename T>(T&& V) -> decltype(auto) { return Forward<T>(V); });
	}
	else
	{
		// 파라미터가 여러 개인 델리게이트는 지원하지 않음
		static_assert(TFalse<DelegateType>::Value);
	}
}

template <typename T, CDelegate DelegateType, typename PredicateType, typename TransformFuncType>
auto MakeFutureFromDelegate(DelegateType& Delegate, PredicateType&& Predicate, TransformFuncType&& TransformFunc)
{
	auto [Promise, Future] = MakeShareablePromise<T>();

	FutureDetails::BindOneOff(Delegate, [
			Promise,
			Predicate = Forward<PredicateType>(Predicate),
			TransformFunc = Forward<TransformFuncType>(TransformFunc)
		]<typename... ArgTypes>(ArgTypes&&... Args) mutable
		{
			if (!Predicate(Forward<ArgTypes>(Args)...))
			{
				return;
			}

			if constexpr (std::is_same_v<T, void>)
			{
				TransformFunc(Forward<ArgTypes>(Args)...);
				Promise.SetValue();
			}
			else
			{
				Promise.SetValue(TransformFunc(Forward<ArgTypes>(Args)...));
			}
		});

	return MoveTemp(Future);
}

template <typename MulticastDelegateType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);
	MulticastDelegate.Add(Delegate);
	return Ret;
}

template <typename T, CMulticastDelegate MulticastDelegateType, typename PredicateType, typename TransformFuncType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate, PredicateType&& Predicate, TransformFuncType&& TransformFunc)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate<T>(Delegate, Forward<PredicateType>(Predicate), Forward<TransformFuncType>(TransformFunc));
	MulticastDelegate.Add(Delegate);
	return Ret;
}

template <CDelegate DelegateType>
auto operator co_await(DelegateType& Delegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(Delegate)};
}

template <CMulticastDelegate MulticastDelegateType>
auto operator co_await(MulticastDelegateType& MulticastDelegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(MulticastDelegate)};
}


inline TCancellableFuture<AGameStateBase*> WaitForGameState(UWorld* World)
{
	check(IsValid(World));

	if (IsValid(World->GetGameState()))
	{
		return World->GetGameState();
	}

	return MakeFutureFromDelegate(World->GameStateSetEvent);
}


inline TCancellableFuture<void> WaitForSeconds(UWorld* World, float Seconds)
{
	check(IsValid(World));

	FTimerDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, Delegate, Seconds, false);

	return Ret;
}


template <typename SoftObjectType>
TCancellableFuture<SoftObjectType*> RequestAsyncLoad(const TSoftObjectPtr<SoftObjectType>& SoftPointer)
{
	if (SoftPointer.IsNull())
	{
		return nullptr;
	}
	
	if (SoftPointer.IsValid())
	{
		return SoftPointer.Get();
	}
	
	FStreamableDelegate Delegate;
	auto Ret = MakeFutureFromDelegate<SoftObjectType*>(
		Delegate,
		[]() { return true; },
		[SoftPointer]() { return SoftPointer.Get(); });
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPointer.ToSoftObjectPath(), Delegate);
	return Ret;
}
