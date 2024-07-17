// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <coroutine>

#include "CoreMinimal.h"


template <typename FromType, typename... ToTypes>
constexpr bool IsConvertibleV = (std::is_convertible_v<FromType, ToTypes> || ...);


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

	TCancellableFutureAwaitable(FutureType&& InFuture)
		: Future(MoveTemp(InFuture))
	{
	}

	TCancellableFutureAwaitable(const TCancellableFutureAwaitable&) = delete;
	TCancellableFutureAwaitable& operator=(const TCancellableFutureAwaitable&) = delete;

	bool await_ready() const
	{
		return Future.IsReady();
	}

	void await_suspend(std::coroutine_handle<> Handle)
	{
		Future.Then([this, Handle](typename FutureType::ReturnType&& Arg)
		{
			Ret.Emplace(MoveTemp(Arg));
			Handle.resume();
		});
	}

	typename FutureType::ReturnType await_resume()
	{
		return Ret ? MoveTemp(*Ret) : Future.ConsumeValue();
	}

private:
	FutureType Future;
	TOptional<typename FutureType::ReturnType> Ret;
};


template <typename... Types>
TCancellableFutureAwaitable<Types...> operator co_await(TCancellableFuture<Types...>&& Future)
{
	return { MoveTemp(Future) };
}


namespace FutureDetails
{
	template <typename, template <typename...> typename>
	struct TIsInstantiationOf : std::false_type
	{
	};

	template <typename... Args, template <typename...> typename T>
	struct TIsInstantiationOf<T<Args...>, T> : std::true_type
	{
	};

	template <typename T, template <typename...> typename Template>
	inline constexpr bool TIsInstantiationOf_V = TIsInstantiationOf<T, Template>::value;


	template <template <typename...> typename TypeList, typename SignatureType>
	struct TFuncParamTypesToTypeList;

	template <template <typename...> typename TypeList, typename RetType, typename... ParamTypes>
	struct TFuncParamTypesToTypeList<TypeList, RetType(ParamTypes...)>
	{
		using Type = TypeList<ParamTypes...>;
	};


	template <typename TypeList>
	struct TFirstInTypeList;

	template <template <typename...> typename TypeList, typename First, typename... Types>
	struct TFirstInTypeList<TypeList<First, Types...>>
	{
		using Type = First;
	};


	template <typename TypeList>
	constexpr int32 TTypeListCount_V = 0;

	template <template <typename...> typename TypeList, typename... Types>
	constexpr int32 TTypeListCount_V<TypeList<Types...>> = sizeof...(Types);


	template <typename T>
	concept CDelegate = TIsInstantiationOf_V<T, TDelegate>;

	template <typename T>
	concept CMulticastDelegate = TIsInstantiationOf_V<T, TMulticastDelegate>;


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


template <FutureDetails::CDelegate DelegateType>
auto MakeFutureFromDelegate(DelegateType& Delegate)
{
	using ParamTypeTuple = typename FutureDetails::TFuncParamTypesToTypeList<TTuple, typename DelegateType::TFuncType>::Type;

	constexpr int32 ParamCount = FutureDetails::TTypeListCount_V<ParamTypeTuple>;

	if constexpr (ParamCount == 0)
	{
		auto [Promise, Future] = MakeShareablePromise<void>();
		FutureDetails::BindOneOff(Delegate, [Promise]() mutable { Promise.SetValue(); });
		return MoveTemp(Future);
	}
	else if constexpr (ParamCount == 1)
	{
		using ResultType = typename FutureDetails::TFirstInTypeList<ParamTypeTuple>::Type;
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


template <FutureDetails::CMulticastDelegate MulticastDelegateType>
auto MakeFutureFromDelegate(MulticastDelegateType& MulticastDelegate)
{
	typename MulticastDelegateType::FDelegate Delegate;
	auto Ret = MakeFutureFromDelegate(Delegate);
	MulticastDelegate.Add(Delegate);
	return Ret;
}
