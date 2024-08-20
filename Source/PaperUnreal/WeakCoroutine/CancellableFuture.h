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
	/**
	 * SetValue가 호출 전에 Promise가 파괴된 경우
	 */
	static UCancellableFutureError* PromiseNotFulfilled()
	{
		return NewError<UCancellableFutureError>(TEXT("PromiseNotFulfilled"));
	}

	/**
	 * Promise가 SetValue를 포기한 경우 (Cancel을 호출한 경우)
	 */
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
	void SetValue(U&& Value) requires std::is_constructible_v<TFailableResult<ResultType>, U&&>
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


/**
 * std::future와 비슷하지만 TFailableResult를 통한 에러 핸들링 기능을 추가한 클래스
 * 코루틴과의 접목을 염두에 두고 작성되었으므로 Then Chain을 구성하는 것이 불가능합니다(이 경우 코루틴을 사용해야 합니다)
 * 
 * 에러 핸들링 기능에 대한 설명은 TFailableResult를 참고해주세요
 *
 * 한 번 소모되면 다시 값을 가져올 수 없습니다. (Then 또는 ConsumeValue 중 하나를 한 번만 호출할 수 있음)
 */
template <typename T>
class TCancellableFuture
{
public:
	using StateType = TCancellableFutureState<T>;
	using ResultType = typename StateType::ResultType;

	TCancellableFuture() requires std::is_void_v<T>
		: State(MakeShared<StateType>())
	{
		State->SetValue();
	}

	TCancellableFuture(const TSharedRef<StateType>& InState)
		: State(InState)
	{
	}

	template <typename U>
	TCancellableFuture(U&& ReadyValue)
		requires requires(StateType State) { State.SetValue(Forward<U>(ReadyValue)); }
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


/**
 * std::promise와 비슷하지만 TFailableResult를 통한 에러 핸들링 기능을 구현한 클래스
 * 
 * @see TCancellableFuture
 */
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
		// TODO 만약에 프로그램이 종료될 때까지 약속이 지켜지지 않은 경우
		// 엔진이 종료되면서 이 destructor가 호출되는데 이 때 엔진 종료 중이기 때문에 NewObject가 실패함
		// 그래서 일단 이 경우에는 PromiseNotFulfilled를 주지 않도록 막음 근데 문제는 이러면 이 에러에 의존하는
		// 특정 리소스의 clean up이 제대로 이루어지지 않을 수 있음 현재는 그런 경우는 없지만 추후 수정 필요
		if (State && !IsEngineExitRequested())
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

/**
 * TCancellablePromise와 달리 이 클래스는 복사가 가능하며 여러 장소에서 SetValue를 호출하는 것이 가능합니다.
 * 첫 SetValue로 설정된 값이 Future로 전달되며 그 이후의 SetValue는 모두 무시됩니다.
 */
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


template <typename FutureType>
class TCancellableFutureAwaitable
{
public:
	using ResultType = typename std::decay_t<FutureType>::ResultType;

	TCancellableFutureAwaitable(FutureType InFuture)
		requires std::is_lvalue_reference_v<FutureType>
		: Future(InFuture)
	{
		static_assert(CErrorReportingAwaitable<TCancellableFutureAwaitable>);
	}

	TCancellableFutureAwaitable(FutureType&& InFuture)
		requires !std::is_reference_v<FutureType>
		: Future(MoveTemp(InFuture))
	{
		static_assert(CErrorReportingAwaitable<TCancellableFutureAwaitable>);
	}

	TCancellableFutureAwaitable(TCancellableFutureAwaitable&&) = default;

	bool await_ready() const
	{
		// 이미 co_await을 끝낸 Future를 또 co_wait한 경우 여기 들어올 수 있음
		check(!Future.IsConsumed());
		return Future.IsReady();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		Future.Then([Handle = Forward<HandleType>(Handle), bAborted = bAborted](auto&)
		{
			if (!*bAborted)
			{
				Handle.resume();
			}
		});
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(Future).ConsumeValue();
	}

	void await_abort()
	{
		*bAborted = true;
	}

protected:
	FutureType Future;

private:
	TSharedRef<bool> bAborted = MakeShared<bool>(false);
};

template <typename T>
TCancellableFutureAwaitable(T&&) -> TCancellableFutureAwaitable<T>;

/**
 * co_await Future; 지원용
 */
template <typename T>
auto operator co_await(TCancellableFuture<T>& Future) { return TCancellableFutureAwaitable{Future}; }

/**
 * co_await MoveTemp(Future); 지원용
 */
template <typename T>
auto operator co_await(TCancellableFuture<T>&& Future) { return TCancellableFutureAwaitable{MoveTemp(Future)}; }


/**
 * Promise에서 Future를 쉽게 꺼내기 위한 편의 함수
 */
template <typename T>
TTuple<TCancellablePromise<T>, TCancellableFuture<T>> MakePromise()
{
	TCancellablePromise<T> Ret;
	TCancellableFuture<T> Future = Ret.GetFuture();
	return MakeTuple(MoveTemp(Ret), MoveTemp(Future));
}


/**
 * Promise에서 Future를 쉽게 꺼내기 위한 편의 함수
 */
template <typename T>
TTuple<TShareableCancellablePromise<T>, TCancellableFuture<T>> MakeShareablePromise()
{
	TShareableCancellablePromise<T> Ret;
	return MakeTuple(Ret, Ret.GetFuture());
}


/**
 * TDelegate가 Execute될 때 Execute에 대한 인자를 반환으로 하는 TCancellableFuture를 반환합니다.
 *
 * 예시)
 * DECLARE_DELEGATE_OneParam(FSomeDelegate, int32);
 * FSomeDelegate Delegate;
 * 
 * TCancellableFuture<int32> Future = MakeFutureFromDelegate(Delegate);
 * Delegate.Execute(42);
 * check(Future.ConsumeValue().GetResult() == 42);
 *
 * Simple Delegate에 대해서는 TCancellableFuture<void>를 반환합니다.
 * 파라미터가 2개 이상인 Delegate에 대해서는 TTuple로 묶어서 반환합니다.
 *
 * 주의 : 모든 타입은 Decay된 이후 TCancellableFuture로 만들어집니다.
 * (즉 Delegate 파라미터 타입의 const, reference 등은 없어짐)
 */
template <CDelegate DelegateType>
auto MakeFutureFromDelegate(DelegateType& Delegate)
{
	constexpr int32 ParamCount = TDelegateTypeTraits<DelegateType>::ParamCount;
	using RetType = typename TDelegateTypeTraits<DelegateType>::DecayedParamTypeOrTuple;

	if constexpr (ParamCount == 0)
	{
		return MakeFutureFromDelegate(Delegate,
			[]() { return true; },
			[]() { return; });
	}
	else if constexpr (ParamCount == 1)
	{
		return MakeFutureFromDelegate(Delegate,
			[](auto) { return true; },
			[]<typename T>(T&& V) -> decltype(auto) { return Forward<T>(V); });
	}
	else
	{
		return MakeFutureFromDelegate(Delegate,
			[](const auto&...) { return true; },
			[]<typename... ArgTypes>(ArgTypes&&... Args) -> decltype(auto) { return RetType{Forward<ArgTypes>(Args)...}; });
	}
}

/**
 * 
 * TDelegate가 Execute될 때 Execute에 대한 인자를 Transform한 것을 반환으로 하는 TCancellableFuture를 반환합니다.
 *
 * 예시)
 * DECLARE_DELEGATE_OneParam(FSomeDelegate, int32);
 * FSomeDelegate Delegate;
 *
 * const auto FilterGreaterThan10 = [](int32 Value) { return Value > 10; }; // 10이하는 걸러짐 (false 반환이 걸러지는 쪽)
 * const auto TransformToString = [](int32 Value) { return FString::FromInt(Value); };
 * 
 * TCancellableFuture<FString> Future = MakeFutureFromDelegate(Delegate, FilterGreaterThan10, TransformToString);
 * Delegate.Execute(1);
 * Delegate.Execute(5);
 * Delegate.Execute(15);
 * check(Future.ConsumeValue().GetResult() == TEXT("15"));
 */
template <CDelegate DelegateType, typename PredicateType, typename TransformFuncType>
auto MakeFutureFromDelegate(DelegateType& Delegate, PredicateType&& Predicate, TransformFuncType&& TransformFunc)
{
	using RetType = decltype
	(
		std::declval<typename TDelegateTypeTraits<DelegateType>::ParamTypeTuple>()
		.ApplyAfter([&TransformFunc]<typename... ArgTypes>(ArgTypes&&... Args)
		{
			return TransformFunc(Forward<ArgTypes>(Args)...);
		})
	);
	
	auto [Promise, Future] = MakeShareablePromise<RetType>();

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

			if constexpr (std::is_same_v<RetType, void>)
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

/**
 * MakeFutureFromDelegate의 멀티캐스트 델리게이트 버전 (위 함수 주석 참고 부탁드립니다)
 */
template <typename MulticastDelegateType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);
	MulticastDelegate.Add(Delegate);
	return Ret;
}

/**
 * MakeFutureFromDelegate의 멀티캐스트 델리게이트 버전 (위 함수 주석 참고 부탁드립니다)
 */
template <CMulticastDelegate MulticastDelegateType, typename PredicateType, typename TransformFuncType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate, PredicateType&& Predicate, TransformFuncType&& TransformFunc)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate, Forward<PredicateType>(Predicate), Forward<TransformFuncType>(TransformFunc));
	MulticastDelegate.Add(Delegate);
	return Ret;
}

/**
 * co_await Delegate; 지원용
 */
template <CDelegate DelegateType>
auto operator co_await(DelegateType& Delegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(Delegate)};
}

/**
 * co_await MulticastDelegate; 지원용
 */
template <CMulticastDelegate MulticastDelegateType>
auto operator co_await(MulticastDelegateType& MulticastDelegate)
{
	return TCancellableFutureAwaitable{MakeFutureFromDelegate(MulticastDelegate)};
}


/**
 * 월드에 GameState가 존재하는 시점에 완료되는 TCancellableFuture를 반환합니다.
 */
inline TCancellableFuture<AGameStateBase*> WaitForGameState(UWorld* World)
{
	check(IsValid(World));

	if (IsValid(World->GetGameState()))
	{
		return World->GetGameState();
	}

	return MakeFutureFromDelegate(World->GameStateSetEvent);
}


/**
 * FTimerManager를 사용해 Seconds 후에 완료되는 TCancellableFuture를 반환합니다.
 */
inline TCancellableFuture<void> WaitForSeconds(UWorld* World, float Seconds)
{
	check(IsValid(World));

	FTimerDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, Delegate, Seconds, false);

	return Ret;
}


auto RequestAsyncLoadImpl(const auto& SoftPointer)
{
	using OutputType = decltype(SoftPointer.Get());
	using RetType = TCancellableFuture<OutputType>;

	if (SoftPointer.IsNull())
	{
		return RetType{nullptr};
	}

	if (SoftPointer.IsValid())
	{
		return RetType{SoftPointer.Get()};
	}

	FStreamableDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(
		Delegate,
		[]() { return true; },
		[SoftPointer]() { return SoftPointer.Get(); });
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPointer.ToSoftObjectPath(), Delegate);
	return Ret;
}

/**
 * FStreamableManager를 사용해 애셋 로드 이후 완료되는 TCancellableFuture를 반환합니다.
 */
template <typename SoftType>
TCancellableFuture<UClass*> RequestAsyncLoad(const TSoftClassPtr<SoftType>& SoftPointer)
{
	return RequestAsyncLoadImpl(SoftPointer);
}

/**
 * FStreamableManager를 사용해 애셋 로드 이후 완료되는 TCancellableFuture를 반환합니다.
 */
template <typename SoftType>
TCancellableFuture<SoftType*> RequestAsyncLoad(const TSoftObjectPtr<SoftType>& SoftPointer)
{
	return RequestAsyncLoadImpl(SoftPointer);
}
