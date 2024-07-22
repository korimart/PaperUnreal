// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/Core/WeakCoroutine/TypeTraits.h"


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


enum class EDefaultFutureError
{
	PromiseNotFulfilled,
	Cancelled,
	InvalidObject,
};


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
		constexpr bool bSettingNonError = std::is_convertible_v<U, ResultType>;

		if constexpr (bUObject && bSettingNonError)
		{
			if (!IsValid(Value))
			{
				SetValue(EDefaultFutureError::InvalidObject);
				return;
			}
		}

		using ReturnInPlaceType = typename TFirstConvertibleType<U, ResultType, ErrorTypes...>::Type;
		using ReturnStorageInPlaceType = std::conditional_t<bUObject && bSettingNonError, WeakResultType, ReturnInPlaceType>;

		if (Callback)
		{
			Callback(ReturnType{TInPlaceType<ReturnInPlaceType>{}, Forward<U>(Value)});
		}
		else
		{
			Ret.Emplace(ReturnStorageType{TInPlaceType<ReturnStorageInPlaceType>{}, Forward<U>(Value)});
		}
	}

	bool HasValue() const
	{
		return Ret.IsSet();
	}

	ReturnType&& ConsumeValue() [[msvc::lifetimebound]] requires !bUObject
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
			if (WeakPtr->IsValid())
			{
				return ReturnType{TInPlaceType<ResultType>{}, WeakPtr->Get()};
			}
			else
			{
				return ReturnType{TInPlaceType<EDefaultFutureError>{}, EDefaultFutureError::InvalidObject};
			}
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


template <typename ResultVariantType>
class TCancellableFutureReturn
{
	using NonErrorType = typename TFirstInTypeList<ResultVariantType>::Type;

public:
	TCancellableFutureReturn(ResultVariantType&& ResultVariant)
		: Variant(MoveTemp(ResultVariant))
	{
	}

	explicit operator bool() const
	{
		return Variant.GetIndex() == 0;
	}

	const auto& Get() const & { return Variant.template Get<NonErrorType>(); }
	auto& Get() & { return Variant.template Get<NonErrorType>(); }

	template <typename T>
	auto TryGet() const { return Variant.template TryGet<T>(); }

private:
	ResultVariantType Variant;
};


template <typename T, typename... ErrorTypes>
class TCancellableFuture
{
public:
	using StateType = TCancellableFutureState<T, EDefaultFutureError, ErrorTypes...>;
	using ResultType = typename StateType::ResultType;
	using ReturnType = TCancellableFutureReturn<typename StateType::ReturnType>;

	TCancellableFuture(const TSharedRef<StateType>& InState)
		: State(InState)
	{
	}

	TCancellableFuture() requires std::is_same_v<T, void>
		: State(MakeShared<StateType>())
	{
		State->SetValue();
	}

	template <typename U>
	TCancellableFuture(U&& ReadyValue) requires IsConvertibleV<U, T, ErrorTypes...>
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
	void Then(FuncType&& Func)
	{
		check(!IsConsumed());

		auto FuncTakingFutureReturn = [Func = Forward<FuncType>(Func)]<typename U>(U&& FromState)
		{
			Func(ReturnType{Forward<U>(FromState)});
		};

		if (State->HasValue())
		{
			FuncTakingFutureReturn(State->ConsumeValue());
		}
		else
		{
			State->ConsumeValueOnArrival(MoveTemp(FuncTakingFutureReturn));
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
		std::exchange(State, nullptr)->SetValue();
	}

	template <typename U>
	void SetValue(U&& Value) requires IsConvertibleV<U, T, EDefaultFutureError, ErrorTypes...>
	{
		check(!IsSet());
		std::exchange(State, nullptr)->SetValue(Forward<U>(Value));
	}

	void Cancel()
	{
		check(!IsSet());
		std::exchange(State, nullptr)->SetValue(EDefaultFutureError::Cancelled);
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


template <typename T, typename... ErrorTypes, typename U>
TCancellableFuture<T, ErrorTypes...> MakeReadyFuture(U&& ReadyValue) requires IsConvertibleV<U, T, ErrorTypes...>
{
	return TCancellableFuture<T, ErrorTypes...>{Forward<U>(ReadyValue)};
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

template <CMulticastDelegate MulticastDelegateType>
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


#include "Engine/AssetManager.h"


// TODO 바로 수령하지 않으면 nullptr를 반환할 수 있음
// TODO 로드 실패했으면 에러를 반환하기
template <typename SoftObjectType>
TCancellableFuture<SoftObjectType*> RequestAsyncLoad(const TSoftObjectPtr<SoftObjectType>& SoftPointer)
{
	FStreamableDelegate Delegate;
	auto Ret = MakeFutureFromDelegate<SoftObjectType*>(
		Delegate,
		[]() { return true; },
		[SoftPointer]() { return SoftPointer.Get(); });
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPointer.ToSoftObjectPath(), Delegate);
	return Ret;
}
