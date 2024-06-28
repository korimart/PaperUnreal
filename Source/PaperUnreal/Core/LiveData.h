// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"


template <typename ValueType>
class TLiveData
{
public:
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

	TWeakAwaitable<ValueType> WaitForValue(UObject* Lifetime)
	{
		if (Value)
		{
			return *Value;
		}

		// TODO TLiveData는 Broadcast와 달리 Value를 보관하고 있으므로
		// 레퍼런스만 넘겨서 복사를 회피할 수 있음
		return WaitForBroadcast(Lifetime, OnChanged);
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValue(T&& Right)
	{
		if (bExecutingCallbacks || Value != Right)
		{
			Value.Emplace(Forward<T>(Right));
			NotifyOnExecutionEnd();
		}
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	bool SetValueSilent(T&& Right)
	{
		if (Value != Right)
		{
			Value.Emplace(Forward<T>(Right));
			return true;
		}
		return false;
	}

	const TOptional<ValueType>& GetValue() const requires !std::is_pointer_v<ValueType>
	{
		return Value;
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
	bool bDeferredExecutionPending = false;

	void NotifyOnExecutionEnd()
	{
		if (bExecutingCallbacks)
		{
			bDeferredExecutionPending = true;
			return;
		}

		const TOptional<ValueType> Copy = Value;
		bExecutingCallbacks = true;
		if (Value) OnChanged.Broadcast(*Value);
		bExecutingCallbacks = false;
		const bool bValueChangedWhileExecutingCallbacks = Copy != Value;

		if (bDeferredExecutionPending && bValueChangedWhileExecutingCallbacks)
		{
			bDeferredExecutionPending = false;
			NotifyOnExecutionEnd();
		}
	}
};


template <typename ValueType>
class TLiveDataView
{
public:
	TLiveDataView(TLiveData<ValueType>& InLiveData)
		: LiveData(InLiveData)
	{
	}

	TWeakAwaitable<ValueType> WaitForValue(UObject* Lifetime)
	{
		return LiveData.WaitForValue(Lifetime);
	}

	decltype(auto) GetValue() const
	{
		return LiveData.GetValue();
	}

	template <typename T>
	operator T()
	{
		return LiveData;
	}

private:
	TLiveData<ValueType>& LiveData;
};


#define DECLARE_LIVE_DATA_AND_GETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };\
	void Set##Name(const Type& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
