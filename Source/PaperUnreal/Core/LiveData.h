// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"


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

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValue(T&& Right)
	{
		check(!bExecutingCallbacks);
		if (SetValueSilent(Forward<T>(Right)))
		{
			Notify();
		}
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
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
};


// TODO Object 타입에 대한 처리가 필요함 Cache 되어 있는 동안에 파괴될 수 있음
template <typename InValueType>
class TCachingLiveData
	// 베이스 포인터 또는 레퍼런스로 캐스팅을 허용해서 베이스 함수 호출을 허용하면 클래스 고장나므로 private 상속 (캐싱이 되지 않음)
	// 캐싱을 회피하고 싶은 상황이 생기면 explicit casting을 허용하거나 회피하는 함수를 따로 작성해야함
	: private TLiveData<InValueType>
{
public:
	using ValueType = InValueType;

	using Super = TLiveData<ValueType>;
	using Super::Super;
	using Super::operator=;
	using Super::WaitForValue;
	using Super::SetValue;
	using Super::Notify;
	using Super::GetValue;
	using Super::operator const TOptional<ValueType>&;
	using Super::operator ValueType;

	TValueStream<ValueType> CreateStream()
	{
		return CreateMulticastValueStream(History, this->OnChanged);
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	bool SetValueSilent(T&& NewValue)
	{
		History.Add(NewValue);
		return Super::SetValueSilent(Forward<T>(NewValue));
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValueIfNotInHistory(T&& NewValue)
	{
		if (!History.Contains(NewValue))
		{
			SetValue(Forward<T>(NewValue));
		}
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValueIfNotInHistory(const TArray<T>& NewValue)
	{
		for (const T& Each : NewValue)
		{
			if (AllValid(Each))
			{
				SetValueIfNotInHistory(Each);
			}
		}
	}

	const TArray<ValueType>& GetHistory() const
	{
		return History;
	}

private:
	TArray<ValueType> History;
};


template <typename LiveDataType>
concept CLiveData = requires(LiveDataType LiveData)
{
	typename LiveDataType::ValueType;
	{ LiveData.WaitForValue() } -> std::same_as<TWeakAwaitable<typename LiveDataType::ValueType>>;
	{ LiveData.CreateStream() } -> std::same_as<TValueStream<typename LiveDataType::ValueType>>;
	{ LiveData.GetValue() };
};


template <typename LiveDataType>
concept CCachingLiveData = requires(LiveDataType LiveData)
{
	requires CLiveData<LiveDataType>;
	{ LiveData.GetHistory() };
};


template <CLiveData LiveDataType>
class TLiveDataView
{
public:
	TLiveDataView(LiveDataType& InLiveData)
		: LiveData(InLiveData)
	{
	}

	TWeakAwaitable<typename LiveDataType::ValueType> WaitForValue() { return LiveData.WaitForValue(); }
	TValueStream<typename LiveDataType::ValueType> CreateStream() { return LiveData.CreateStream(); }
	decltype(auto) GetValue() const { return LiveData.GetValue(); }

	TArray<typename LiveDataType::ValueType> GetHistory() const requires CCachingLiveData<LiveDataType>
	{
		return LiveData.GetHistory();
	}

private:
	LiveDataType& LiveData;
};


// TLiveData Declarations
#define DECLARE_LIVE_DATA_AND_GETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };


#define DECLARE_LIVE_DATA_AND_GETTER_WITH_DEFAULT(Type, Name, Default)\
	private: TLiveData<Type> Name{Default};\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<TLiveData<Type>> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER_WITH_DEFAULT(Type, Name, Default)\
	private: TLiveData<Type> Name{Default};\
	public:\
	TLiveDataView<TLiveData<Type>> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
// ~TLiveData Declarations


// TCachingLiveData Declarations
#define DECLARE_CACHING_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TCachingLiveData<Type> Name;\
	public:\
	TLiveDataView<TCachingLiveData<Type>> Get##Name() { return Name; };\
// ~TCachingLiveData Declarations
