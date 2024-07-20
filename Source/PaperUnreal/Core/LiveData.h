// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeakCoroutine/ValueStream.h"


template <typename T>
concept CInequalityComparable = requires(T Arg0, T Arg1)
{
	Arg0 != Arg1;
};


// TODO Object 타입에 대한 처리가 필요함 Cache 되어 있는 동안에 파괴될 수 있음
template <typename InValueType>
class TLiveData
{
public:
	using ValueType = InValueType;

	TLiveData() = default;

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TLiveData(T&& Other)
		: Value(Forward<T>(Other))
	{
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TLiveData& operator=(T&& Other)
	{
		SetValue(Forward<T>(Other));
		return *this;
	}

	template <typename FuncType>
	void Observe(UObject* Lifetime, FuncType&& Func)
	{
		if (Value)
		{
			Func(*Value);
		}

		OnChanged.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void Observe(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		if (Value)
		{
			Func(*Value);
		}

		OnChanged.Add(FOnChanged::FDelegate::CreateSPLambda(Lifetime, Forward<FuncType>(Func)));
	}

	friend TCancellableFutureAwaitable<ValueType> operator co_await(TLiveData& LiveData)
	{
		auto Future = [&]() -> TCancellableFuture<ValueType>
		{
			if (LiveData.Value)
			{
				return *LiveData.Value;
			}

			return MakeFutureFromDelegate(LiveData.OnChanged);
		}();

		return Future;
	}

	TValueStream<ValueType> CreateStream()
	{
		if (Value)
		{
			return CreateMulticastValueStream(TArray<ValueType>{*Value}, OnChanged);
		}
		return CreateMulticastValueStream(TArray<ValueType>{}, OnChanged);
	}

	template <typename T> requires CInequalityComparable<ValueType> && std::is_convertible_v<T, ValueType>
	void SetValue(T&& Right)
	{
		check(!bExecutingCallbacks);
		if (SetValueSilent(Forward<T>(Right)))
		{
			Notify();
		}
	}

	template <typename T> requires CInequalityComparable<ValueType> && std::is_convertible_v<T, ValueType>
	bool SetValueSilent(T&& Right)
	{
		check(!bExecutingCallbacks);
		if (Value != Right)
		{
			Value.Emplace(Forward<T>(Right));
			return true;
		}
		return false;
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValueAlways(T&& Right)
	{
		Value.Emplace(Forward<T>(Right));
		Notify();
	}

	void Notify()
	{
		if (Value)
		{
			check(!bExecutingCallbacks);
			bExecutingCallbacks = true;
			OnChanged.Broadcast(*Value);
			bExecutingCallbacks = false;
		}
	}

	TOptional<ValueType>& GetValue() requires !std::is_pointer_v<ValueType>
	{
		return Value;
	}

	ValueType* operator->() requires !std::is_pointer_v<ValueType>
	{
		return &*Value;
	}

	ValueType& operator*() requires !std::is_pointer_v<ValueType>
	{
		return *Value;
	}

	const TOptional<ValueType>& GetValue() const requires !std::is_pointer_v<ValueType>
	{
		return Value;
	}

	const ValueType* operator->() const requires !std::is_pointer_v<ValueType>
	{
		return &*Value;
	}

	const ValueType& operator*() const requires !std::is_pointer_v<ValueType>
	{
		return *Value;
	}

	operator const TOptional<ValueType>&() const requires !std::is_pointer_v<ValueType>
	{
		return GetValue();
	}

	ValueType GetValue() const requires std::is_pointer_v<ValueType>
	{
		return Value ? *Value : nullptr;
	}

	operator ValueType() const requires std::is_pointer_v<ValueType>
	{
		return GetValue();
	}

protected:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, const ValueType&);
	FOnChanged OnChanged;

	TOptional<ValueType> Value;
	bool bExecutingCallbacks = false;
};


template <typename PredicateType, typename ValueType>
concept CPredicate = requires(PredicateType Predicate, ValueType Value)
{
	{ Predicate(Value) } -> std::same_as<bool>;
};


template <typename T, typename U>
concept CEqualityComparable = requires(T Arg0, U Arg1)
{
	Arg0 == Arg1;
};


template <typename ValueType>
class TLiveDataView
{
public:
	TLiveDataView(TLiveData<ValueType>& InLiveData)
		: LiveData(InLiveData)
	{
	}

	friend TCancellableFutureAwaitable<ValueType> operator co_await(TLiveDataView LiveDataView)
	{
		return operator co_await(LiveDataView.LiveData);
	}

	template <CEqualityComparable<ValueType> ArgType>
	TCancellableFuture<ValueType, EValueStreamError> If(ArgType&& OfThis)
	{
		return FirstInStream(CreateStream(), [OfThis = Forward<ArgType>(OfThis)](const ValueType& Value) { return Value == OfThis; });
	}

	template <CPredicate<ValueType> PredicateType>
	TCancellableFuture<ValueType, EValueStreamError> If(PredicateType&& Predicate)
	{
		return FirstInStream(CreateStream(), [Predicate = Forward<PredicateType>(Predicate)](const ValueType& Value) { return Predicate(Value); });
	}

	TValueStream<ValueType> CreateStream() { return LiveData.CreateStream(); }

	decltype(auto) GetValue() { return LiveData.GetValue(); }
	decltype(auto) GetValue() const { return LiveData.GetValue(); }
	decltype(auto) operator*() { return *LiveData; }
	decltype(auto) operator*() const { return *LiveData; }

	template <typename FuncType>
	void Observe(UObject* Lifetime, FuncType&& Func)
	{
		LiveData.Observe(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void Observe(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		LiveData.Observe(Lifetime, Forward<FuncType>(Func));
	}

private:
	TLiveData<ValueType>& LiveData;
};


// TLiveData Declarations
#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER_WITH_DEFAULT(Type, Name, Default)\
	private: TLiveData<Type> Name{Default};\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
// ~TLiveData Declarations
