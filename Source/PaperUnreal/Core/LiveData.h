// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"


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

	TWeakAwaitable<ValueType> WaitForValue()
	{
		if (Value)
		{
			return *Value;
		}

		// TODO TLiveData는 Broadcast와 달리 Value를 보관하고 있으므로
		// 레퍼런스만 넘겨서 복사를 회피할 수 있음
		return WaitForBroadcast(OnChanged);
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


template <typename ValueType>
class TLiveDataView
{
public:
	TLiveDataView(TLiveData<ValueType>& InLiveData)
		: LiveData(InLiveData)
	{
	}

	TWeakAwaitable<ValueType> WaitForValue() { return LiveData.WaitForValue(); }
	TValueStream<ValueType> CreateStream() { return LiveData.CreateStream(); }
	decltype(auto) GetValue() const { return LiveData.GetValue(); }

	template <typename FuncType>
	void Observe(UObject* Lifetime, FuncType&& Func)
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
