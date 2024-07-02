// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"


template <typename T>
bool operator==(const TSet<T>& Left, const TSet<T>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}

	for (const T& Each : Left)
	{
		if (!Right.Contains(Each))
		{
			return false;
		}
	}

	return true;
}

template <typename T>
bool operator!=(const TSet<T>& Left, const TSet<T>& Right)
{
	return !(Left == Right);
}


template <typename ValueType>
class TLiveDataBase
{
public:
	TLiveDataBase() = default;

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TLiveDataBase(T&& Other)
		: Value(Forward<T>(Other))
	{
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TLiveDataBase& operator=(T&& Other)
	{
		SetValue(Forward<T>(Other));
		return *this;
	}

	template <typename ObserverType>
	void ObserveValid(UObject* Lifetime, ObserverType&& Observer)
	{
		if (Value)
		{
			Observer(*Value);
		}

		OnChanged.AddWeakLambda(Lifetime, [Observer = Forward<ObserverType>(Observer)](const ValueType& NewValue)
		{
			Observer(NewValue);
		});
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


template <typename ValueType>
class TLiveData : public TLiveDataBase<ValueType>
{
public:
	using Super = TLiveDataBase<ValueType>;
	using Super::Super;
	using Super::operator=;
};


template <typename ElementType>
class TLiveData<TSet<ElementType>> : public TLiveDataBase<TSet<ElementType>>
{
public:
	using Super = TLiveDataBase<TSet<ElementType>>;
	using Super::Super;
	using Super::operator=;

	template <typename T> requires std::is_convertible_v<T, ElementType>
	void Add(T&& Element)
	{
		check(!this->bExecutingCallbacks);
		if (ElementType* Added = AddSilentImpl(Forward<T>(Element)))
		{
			this->Notify();

			this->bExecutingCallbacks = true;
			OnElementAdded.Broadcast(*Added);
			this->bExecutingCallbacks = false;
		}
	}

	template <typename T> requires std::is_convertible_v<T, ElementType>
	bool AddSilent(T&& Element)
	{
		check(!this->bExecutingCallbacks);
		return !!AddSilentImpl(Forward<T>(Element));
	}

	template <typename FuncType>
	void ObserveEach(UObject* Lifetime, FuncType&& Func)
	{
		if (this->Value)
		{
			this->bExecutingCallbacks = true;
			for (const ElementType& Each : *this->Value)
			{
				Func(Each);
			}
			this->bExecutingCallbacks = false;
		}

		OnElementAdded.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FElementEvent, const ElementType&);
	FElementEvent OnElementAdded;
    
	template <typename T> requires std::is_convertible_v<T, ElementType>
	ElementType* AddSilentImpl(T&& Element)
	{
		if (!this->Value)
		{
			this->Value.Emplace();
			FSetElementId ElementId = this->Value->Add(Forward<T>(Element));
			return &this->Value->operator[](ElementId);
		}

		if (!this->Value->Contains(Element))
		{
			this->Value->Add(Forward<T>(Element));
			FSetElementId ElementId = this->Value->Add(Forward<T>(Element));
			return &this->Value->operator[](ElementId);
		}

		return nullptr;
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

	template <typename ObserverType>
	void ObserveValid(UObject* Lifetime, ObserverType&& Observer)
	{
		LiveData.ObserveValid(Lifetime, Forward<ObserverType>(Observer));
	}
	
	template <typename ObserverType>
	void ObserveEach(UObject* Lifetime, ObserverType&& Observer) requires TIsInstantiationOf_V<ValueType, TSet>
	{
		LiveData.ObserveEach(Lifetime, Forward<ObserverType>(Observer));
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


#define DECLARE_LIVE_DATA_AND_GETTER_WITH_DEFAULT(Type, Name, Default)\
	private: TLiveData<Type> Name{Default};\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TLiveData<Type> Name;\
	public:\
	TLiveDataView<Type> Get##Name() { return Name; };\
	void Set##Name(const Type& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
