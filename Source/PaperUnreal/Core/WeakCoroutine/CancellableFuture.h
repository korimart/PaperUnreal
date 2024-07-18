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
	static constexpr bool bVoid = std::is_same_v<T, void>;
	static constexpr bool bUObject = std::is_base_of_v<UObject, std::remove_pointer_t<std::decay_t<T>>>;
	using WeakResultType = TWeakObjectPtr<std::remove_pointer_t<T>>;

public:
	using ResultType = std::conditional_t<bVoid, std::monostate, T>;
	using ReturnType = TVariant<ResultType, ErrorTypes...>;

	TCancellableFutureState() = default;
	TCancellableFutureState(const TCancellableFutureState&) = delete;
	TCancellableFutureState& operator=(const TCancellableFutureState&) = delete;

	void SetValue() requires bVoid
	{
		SetValue(std::monostate{});
	}

	template <typename U>
	void SetValue(U&& Value) requires IsConvertibleV<U, ResultType, ErrorTypes...>
	{
		using TargetType = typename TFirstConvertibleType<U, ResultType, ErrorTypes...>::Type;

		if (Callback)
		{
			Callback(ReturnType{TInPlaceType<TargetType>{}, Forward<U>(Value)});
		}
		else
		{
			if constexpr (bUObject && std::is_same_v<TargetType, ResultType>)
			{
				Ret.Emplace(ReturnStorageType{TInPlaceType<WeakResultType>{}, Forward<U>(Value)});
			}
			else
			{
				Ret.Emplace(ReturnStorageType{TInPlaceType<TargetType>{}, Forward<U>(Value)});
			}
		}
	}

	bool HasValue() const
	{
		return Ret.IsSet();
	}

	ReturnType&& ConsumeValue() UE_LIFETIMEBOUND requires !bUObject
	{
		check(!bResultConsumed);
		bResultConsumed = true;
		return MoveTemp(*Ret);
	}

	ReturnType ConsumeValue() requires bUObject
	{
		check(!bResultConsumed);
		bResultConsumed = true;
		return CreateReturnTypeFromStorageType(MoveTemp(*Ret));
	}

	template <typename FuncType>
	void ConsumeValueOnArrival(FuncType&& Func)
	{
		check(!bResultConsumed && !Callback);
		bResultConsumed = true;
		Callback = Forward<FuncType>(Func);
	}

private:
	using ReturnStorageType = std::conditional_t<bUObject, TVariant<WeakResultType, ErrorTypes...>, ReturnType>;

	bool bResultConsumed = false;
	TOptional<ReturnStorageType> Ret;
	TUniqueFunction<void(ReturnType&&)> Callback;

	static ReturnType CreateReturnTypeFromStorageType(ReturnStorageType&& Storage)
	{
		if (auto* WeakPtr = Storage.template TryGet<WeakResultType>())
		{
			return ReturnType{TInPlaceType<ResultType>{}, WeakPtr->Get()};
		}
		return CreateReturnTypeFromStorageType<ErrorTypes...>(MoveTemp(Storage));
	}

	template <typename CheckTarget, typename... RemainingTypes>
	static ReturnType CreateReturnTypeFromStorageType(ReturnStorageType&& Storage)
	{
		if (auto* Found = Storage.template TryGet<CheckTarget>())
		{
			return ReturnType{TInPlaceType<CheckTarget>{}, MoveTemp(*Found)};
		}

		if constexpr (sizeof...(RemainingTypes) > 0)
		{
			return CreateReturnTypeFromStorageType<RemainingTypes...>(MoveTemp(Storage));
		}
		else
		{
			check(false);
			return {};
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
	using StateType = TCancellableFutureState<T, EDefaultFutureError, ErrorTypes...>;
	using ResultType = typename StateType::ResultType;
	using ReturnType = typename StateType::ReturnType;

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
	static constexpr bool bVoidResult = std::is_same_v<T, void>;

public:
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
		State->SetValue();
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
	return {MoveTemp(Future)};
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
		using ResultType = std::decay_t<typename TFirstInTypeList<ParamTypeTuple>::Type>;
		auto [Promise, Future] = MakeShareablePromise<ResultType>();
		FutureDetails::BindOneOff(Delegate, [Promise]<typename ArgType>(ArgType&& Arg) mutable { Promise.SetValue(Forward<ArgType>(Arg)); });
		return MoveTemp(Future);
	}
	else
	{
		// 파라미터가 여러 개인 델리게이트는 지원하지 않음
		static_assert(TFalse<DelegateType>::Value);
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
