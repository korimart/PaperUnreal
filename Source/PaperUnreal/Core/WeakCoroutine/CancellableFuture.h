// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/Core/TypeTraits.h"


namespace FutureDetails
{
	template <typename FuncType>
	void BindOneOff(CDelegate auto& Delegate, FuncType&& Func)
	{
		TSharedRef<bool> Life = MakeShared<bool>(false);
		Delegate.BindSPLambda(Life, [Life = Life.ToSharedPtr(), Func = Forward<FuncType>(Func)]<typename... ArgTypes>(ArgTypes&&... Args) mutable
		{
			Func(Forward<ArgTypes>(Args)...);
			Life = nullptr;
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


template <typename T, typename... ErrorTypes>
class TCancellableFutureState
{
public:
	using ResultType = std::conditional_t<std::is_same_v<T, void>, std::monostate, T>;
	using ResultVariantType = TVariant<ResultType, ErrorTypes...>;

	TCancellableFutureState() = default;
	TCancellableFutureState(const TCancellableFutureState&) = delete;
	TCancellableFutureState& operator=(const TCancellableFutureState&) = delete;

	template <typename U>
	void SetValue(U&& Value) requires IsConvertibleV<U, ResultType, ErrorTypes...>
	{
		Result.Emplace(ResultVariantType{TInPlaceType<U>{}, Forward<U>(Value)});
		InvokeResultCallback();
	}

	bool HasValue() const
	{
		return Result.IsSet();
	}

	ResultVariantType&& ConsumeValue() UE_LIFETIMEBOUND
	{
		check(!bResultConsumed);
		bResultConsumed = true;
		return MoveTemp(*Result);
	}

	template <typename FuncType>
	void ConsumeValueOnArrival(FuncType&& Func)
	{
		check(!bResultConsumed && !Callback);
		Callback = Forward<FuncType>(Func);
	}

private:
	bool bResultConsumed = false;
	TOptional<ResultVariantType> Result;
	TUniqueFunction<void(ResultVariantType&&)> Callback;

	void InvokeResultCallback()
	{
		if (Callback)
		{
			Callback(ConsumeValue());
		}
	}
};


enum class EDefaultFutureError
{
	PromiseNotFulfilled,
	Cancelled,
};


template <typename T, typename... ErrorTypes>
class TCancellableFuture
{
public:
	static constexpr bool bVoidResult = std::is_same_v<T, void>;
	using StateType = TCancellableFutureState<T, EDefaultFutureError, ErrorTypes...>;
	using ResultType = typename StateType::ResultType;
	using ReturnType = typename StateType::ResultVariantType;

	TCancellableFuture(const TSharedRef<StateType>& InState)
		: State(InState)
	{
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
	void Then(FuncType&& Func)
	{
		check(!IsConsumed());

		if (State->HasValue())
		{
			Func(State->ConsumeValue());
		}
		else
		{
			State->ConsumeValueOnArrival(Forward<FuncType>(Func));
		}

		State = nullptr;
	}

	ReturnType ConsumeValue()
	{
		check(IsReady());
		ReturnType Ret = State->ConsumeValue();
		State = nullptr;
		return Ret;
	}

private:
	TSharedPtr<StateType> State;
};


template <typename T, typename... ErrorTypes>
class TCancellablePromise
{
public:
	static constexpr bool bVoidResult = std::is_same_v<T, void>;
	using StateType = TCancellableFutureState<T, EDefaultFutureError, ErrorTypes...>;

	TCancellablePromise() = default;
	
	TCancellablePromise(const TCancellablePromise&) = delete;
	TCancellablePromise& operator=(const TCancellablePromise&) = delete;
	
	TCancellablePromise(TCancellablePromise&&) = default;
	TCancellablePromise& operator=(TCancellablePromise&&) = default;

	~TCancellablePromise()
	{
		if (State)
		{
			State->SetValue(EDefaultFutureError::PromiseNotFulfilled);
		}
	}

	bool IsSet() const
	{
		return !State;
	}

	void SetValue() requires bVoidResult
	{
		check(!IsSet());
		State->SetValue(std::monostate{});
		State = nullptr;
	}

	template <typename U>
	void SetValue(U&& Value) requires IsConvertibleV<U, T, ErrorTypes...>
	{
		check(!IsSet());
		State->SetValue(Forward<U>(Value));
		State = nullptr;
	}

	void Cancel()
	{
		check(!IsSet());
		State->SetValue(EDefaultFutureError::Cancelled);
		State = nullptr;
	}

	TCancellableFuture<T, ErrorTypes...> GetFuture()
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


template <typename T, typename... ErrorTypes>
class TShareableCancellablePromise
{
public:
	static constexpr bool bVoidResult = std::is_same_v<T, void>;
	using PromiseType = TCancellablePromise<T, ErrorTypes...>;

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
	void SetValue(U&& Value) requires IsConvertibleV<U, T, ErrorTypes...>
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

	TCancellableFuture<T, ErrorTypes...> GetFuture()
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


template <typename T, typename... ErrorTypes>
class TCancellableFutureAwaitable
{
public:
	using FutureType = TCancellableFuture<T, ErrorTypes...>;
	using ResultType = typename FutureType::ResultType;
	using ReturnType = typename FutureType::ReturnType;

	TCancellableFutureAwaitable(FutureType&& InFuture)
		: Future(MoveTemp(InFuture))
	{
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
		Future.Then([this, Handle = Forward<HandleType>(Handle)](typename FutureType::ReturnType&& Arg)
		{
			Ret.Emplace(MoveTemp(Arg));
			Handle.resume();
		});
	}

	ReturnType await_resume()
	{
		return Ret ? MoveTemp(*Ret) : Future.ConsumeValue();
	}

	const ReturnType& Peek() const
	{
		return *Ret;
	}

private:
	FutureType Future;
	TOptional<ReturnType> Ret;
};


template <typename... Types>
TCancellableFutureAwaitable<Types...> operator co_await(TCancellableFuture<Types...>&& Future)
{
	return { MoveTemp(Future) };
}


template <typename... Types>
TTuple<TCancellablePromise<Types...>, TCancellableFuture<Types...>> MakePromise()
{
	TCancellablePromise<Types...> Ret;
	TCancellableFuture<Types...> Future = Ret.GetFuture();
	return MakeTuple(MoveTemp(Ret), MoveTemp(Future));
}


template <typename... Types>
TTuple<TShareableCancellablePromise<Types...>, TCancellableFuture<Types...>> MakeShareablePromise()
{
	TShareableCancellablePromise<Types...> Ret;
	return MakeTuple(Ret, Ret.GetFuture());
}


template <CDelegate DelegateType>
auto MakeFutureFromDelegate(DelegateType& Delegate)
{
	using ParamTypeTuple = typename TFuncParamTypesToTypeList<TTuple, typename DelegateType::TFuncType>::Type;

	constexpr int32 ParamCount = TTypeListCount_V<ParamTypeTuple>;

	if constexpr (ParamCount == 0)
	{
		auto [Promise, Future] = MakeShareablePromise<void>();
		FutureDetails::BindOneOff(Delegate, [Promise]() mutable { Promise.SetValue(); });
		return MoveTemp(Future);
	}
	else if constexpr (ParamCount == 1)
	{
		using ResultType = typename TFirstInTypeList<ParamTypeTuple>::Type;
		auto [Promise, Future] = MakeShareablePromise<ResultType>();
		FutureDetails::BindOneOff(Delegate, [Promise]<typename ArgType>(ArgType&& Arg) mutable { Promise.SetValue(Forward<ArgType>(Arg)); });
		return MoveTemp(Future);
	}
	else
	{
		auto [Promise, Future] = MakeShareablePromise<ParamTypeTuple>();
		FutureDetails::BindOneOff(Delegate, [Promise]<typename... ArgTypes>(ArgTypes&&... Args) mutable
		{
			Promise.SetValue(ParamTypeTuple{Forward<ArgTypes>(Args)...});
		});
		return MoveTemp(Future);
	}
}


template <CMulticastDelegate MulticastDelegateType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);
	MulticastDelegate.Add(Delegate);
	return Ret;
}
