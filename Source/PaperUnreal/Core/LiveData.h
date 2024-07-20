// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeakCoroutine/TypeTraits.h"
#include "WeakCoroutine/ValueStream.h"


template <typename Derived, typename InValueType>
class TLiveDataBase
{
public:
	using ValueType = InValueType;

	TLiveDataBase() requires std::is_default_constructible_v<ValueType>
		: Value{}
	{
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TLiveDataBase(T&& Init)
		: Value(Forward<T>(Init))
	{
	}

	template <typename T> requires CInequalityComparable<ValueType> && std::is_convertible_v<T, ValueType>
	TLiveDataBase& operator=(T&& NewValue)
	{
		SetValue(Forward<T>(NewValue));
		return *this;
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
			Value = Forward<T>(Right);
			return true;
		}
		return false;
	}

	template <typename T> requires std::is_convertible_v<T, ValueType>
	void SetValueAlways(T&& Right)
	{
		Value = Forward<T>(Right);
		Notify();
	}

	void Notify()
	{
		if (Derived::IsValid(Get()))
		{
			check(!bExecutingCallbacks);
			bExecutingCallbacks = true;
			OnChanged.Broadcast(Derived::ToValid(Get()));
			bExecutingCallbacks = false;
		}
	}

	template <typename FuncType>
	void Observe(UObject* Lifetime, FuncType&& Func)
	{
		auto ValidCheckingFunc = ToValidCheckingFunc(Forward<FuncType>(Func));
		ValidCheckingFunc(Get());
		OnChanged.AddWeakLambda(Lifetime, ValidCheckingFunc);
	}

	template <typename T, typename FuncType>
	void Observe(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		auto ValidCheckingFunc = ToValidCheckingFunc(Forward<FuncType>(Func));
		ValidCheckingFunc(Get());
		OnChanged.Add(FOnChanged::FDelegate::CreateSPLambda(Lifetime, ValidCheckingFunc));
	}

	auto CreateStream()
	{
		auto InitArray = Derived::IsValid(Get())
			? TArray<typename Derived::ValidType>{Derived::ToValid(Get())}
			: TArray<typename Derived::ValidType>{};

		return CreateMulticastValueStream(InitArray, OnChanged,
			[](const ValueType& NewValue) { return Derived::IsValid(NewValue); },
			[](const ValueType& NewValue) { return Derived::ToValid(NewValue); });
	}

	// 이거 허용을 안 하면 LiveData 안에 들어있는 객체의 non const 함수를 호출할 수 없음
	ValueType& Get() [[msvc::lifetimebound]] { return Value; }

	const ValueType& Get() const [[msvc::lifetimebound]] { return Value; }

protected:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, const ValueType&);
	FOnChanged OnChanged;

	ValueType Value;
	bool bExecutingCallbacks = false;

	template <typename FuncType>
	auto ToValidCheckingFunc(FuncType&& Func)
	{
		return [Func = Forward<FuncType>(Func)](const ValueType& Target)
		{
			if (Derived::IsValid(Target))
			{
				Func(Derived::ToValid(Target));
			}
		};
	}
};


template <typename InValueType>
class TLiveData : public TLiveDataBase<TLiveData<InValueType>, InValueType>
{
public:
	using Super = TLiveDataBase<TLiveData, InValueType>;
	using ValueType = typename Super::ValueType;
	using ValidType = ValueType;
	using Super::Super;

	ValidType* operator->() { return &Super::Get(); }
	const ValidType* operator->() const { return &Super::Get(); }
	ValidType& operator*() { return Super::Get(); }
	const ValidType& operator*() const { return Super::Get(); }

private:
	friend class Super;

	static bool IsValid(const ValueType&) { return true; }
	static const ValueType& ToValid(const ValueType& This [[msvc::lifetimebound]]) { return This; }
};


template <typename InValueType>
class TLiveData<TOptional<InValueType>> : private TLiveDataBase<TLiveData<TOptional<InValueType>>, TOptional<InValueType>>
{
public:
	using Super = TLiveDataBase<TLiveData, TOptional<InValueType>>;
	using ValueType = typename Super::ValueType;
	using ValidType = InValueType;

	using Super::Super;
	using Super::SetValue;
	using Super::SetValueSilent;
	using Super::SetValueAlways;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	const ValidType* operator->() const { return &Super::Get().GetValue(); }
	const ValidType& operator*() const { return Super::Get().GetValue(); }

	friend TCancellableFutureAwaitable<ValidType> operator co_await(TLiveData& LiveData)
	{
		return [&]() -> TCancellableFuture<ValidType>
		{
			if (LiveData.Get())
			{
				return *LiveData.Get();
			}

			return MakeFutureFromDelegate(LiveData.OnChanged,
				[](const ValueType& Value) { return Value.IsSet(); },
				[](const ValueType& Value) { return *Value; });
		}();
	}

private:
	friend class Super;

	static bool IsValid(const ValueType& This) { return This.IsSet(); }
	static const auto& ToValid(const ValueType& This [[msvc::lifetimebound]]) { return This.GetValue(); }
};


// TODO Object 타입에 대한 처리가 필요함 Cache 되어 있는 동안에 파괴될 수 있음
template <typename InValueType>
class TLiveData<InValueType*> : private TLiveDataBase<TLiveData<InValueType*>, InValueType*>
{
public:
	using Super = TLiveDataBase<TLiveData, InValueType*>;
	using ValueType = typename Super::ValueType;
	using ValidType = InValueType*;

	using Super::Super;
	using Super::SetValue;
	using Super::SetValueSilent;
	using Super::SetValueAlways;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	ValidType* operator->() const { return Super::Get(); }
	const ValidType& operator*() const { return *Super::Get(); }

	friend TCancellableFutureAwaitable<ValidType> operator co_await(TLiveData& LiveData)
	{
		return [&]() -> TCancellableFuture<ValidType>
		{
			if (::IsValid(LiveData.Get()))
			{
				return LiveData.Get();
			}

			return MakeFutureFromDelegate<ValidType>(LiveData.OnChanged,
				[](const ValueType& Value) { return ::IsValid(Value); },
				[](const ValueType& Value) { return Value; });
		}();
	}

private:
	friend class Super;

	static bool IsValid(const ValueType& Value) { return ::IsValid(Value); }
	static ValidType ToValid(const ValueType& Value) { return Value; }
};


template <typename ValueType>
class TLiveDataView
{
public:
	using ValidType = typename TLiveData<ValueType>::ValidType;

	TLiveDataView(TLiveData<ValueType>& InLiveData)
		: LiveData(InLiveData)
	{
	}

	friend TCancellableFutureAwaitable<ValidType> operator co_await(TLiveDataView LiveDataView)
	{
		return operator co_await(LiveDataView.LiveData);
	}

	template <CEqualityComparable<ValueType> ArgType>
	TCancellableFuture<ValidType, EValueStreamError> If(ArgType&& OfThis)
	{
		return FirstInStream(CreateStream(), [OfThis = Forward<ArgType>(OfThis)](const ValueType& Value) { return Value == OfThis; });
	}

	template <CPredicate<ValueType> PredicateType>
	TCancellableFuture<ValidType, EValueStreamError> If(PredicateType&& Predicate)
	{
		return FirstInStream(CreateStream(), [Predicate = Forward<PredicateType>(Predicate)](const ValueType& Value) { return Predicate(Value); });
	}

	TValueStream<ValidType> CreateStream() { return LiveData.CreateStream(); }

	decltype(auto) Get() { return LiveData.Get(); }
	decltype(auto) Get() const { return LiveData.Get(); }
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
#define REPPED_VALUE_TYPE(Type) std::conditional_t<std::is_pointer_v<Type>, Type, TOptional<Type>>


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name)\
	private: TLiveData<REPPED_VALUE_TYPE(Type)> Name;\
	public:\
	TLiveDataView<REPPED_VALUE_TYPE(Type)> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }


#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER_WITH_DEFAULT(Type, Name, Default)\
	private: TLiveData<REPPED_VALUE_TYPE(Type)> Name{Default};\
	public:\
	TLiveDataView<REPPED_VALUE_TYPE(Type)> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
// ~TLiveData Declarations
