// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"
#include "ValueStream.h"


template <typename Derived, typename InternalStorageType>
class TLiveDataBase
{
public:
	TLiveDataBase() requires std::is_default_constructible_v<InternalStorageType>
		: Value{}
	{
	}

	template <typename T> requires std::is_convertible_v<T, InternalStorageType>
	explicit TLiveDataBase(T&& Init)
		: Value(Forward<T>(Init))
	{
	}

	template <typename T> requires CInequalityComparable<InternalStorageType> && std::is_assignable_v<InternalStorageType&, T>
	TLiveDataBase& operator=(T&& NewValue)
	{
		SetValue(Forward<T>(NewValue));
		return *this;
	}

	template <typename T> requires CInequalityComparable<InternalStorageType> && std::is_assignable_v<InternalStorageType&, T>
	void SetValue(T&& Right)
	{
		check(!bExecutingCallbacks);
		if (SetValueSilent(Forward<T>(Right)))
		{
			Notify();
		}
	}

	template <typename T> requires CInequalityComparable<InternalStorageType> && std::is_assignable_v<InternalStorageType&, T>
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

	template <typename T> requires std::is_assignable_v<InternalStorageType&, T>
	void SetValueAlways(T&& Right)
	{
		Value = Forward<T>(Right);
		Notify();
	}

	void Notify()
	{
		if (Derived::IsValid(Get()))
		{
			GuardCallbackInvocation([&]()
			{
				OnChanged.Broadcast(Derived::ToValid(Get()));
			});
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

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle Observe(FuncType&& Func)
	{
		FDelegateSPHandle Ret;
		Observe(Ret.ToShared(), Forward<FuncType>(Func));
		return Ret;
	}

	auto CreateStream()
	{
		auto InitArray = Derived::IsValid(Get())
			? TArray<typename Derived::ValidType>{Derived::ToValid(Get())}
			: TArray<typename Derived::ValidType>{};

		return CreateMulticastValueStream(InitArray, OnChanged,
			[](const std::decay_t<InternalStorageType>& NewValue) { return Derived::IsValid(NewValue); },
			[](const std::decay_t<InternalStorageType>& NewValue) { return Derived::ToValid(NewValue); });
	}

	// 이거 허용을 안 하면 LiveData 안에 들어있는 객체의 non const 함수를 호출할 수 없음
	InternalStorageType& Get() [[msvc::lifetimebound]] { return Value; }

	const InternalStorageType& Get() const [[msvc::lifetimebound]] { return Value; }

protected:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, const std::decay_t<InternalStorageType>&);
	FOnChanged OnChanged;

	InternalStorageType Value;
	bool bExecutingCallbacks = false;

	template <typename FuncType>
	void GuardCallbackInvocation(const FuncType& Func)
	{
		// 여기 걸리면 콜백 호출 도중에 LiveData를 수정하려고 한 것임
		// Array 순회 도중에 Array를 수정하려고 한 것과 비슷함
		check(!bExecutingCallbacks);

		bExecutingCallbacks = true;
		Func();
		bExecutingCallbacks = false;
	}

	template <typename FuncType>
	static auto ToValidCheckingFunc(FuncType&& Func)
	{
		return [Func = Forward<FuncType>(Func)](const std::decay_t<InternalStorageType>& Target)
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
	using ValueType = InValueType;
	using ValidType = ValueType;

	using Super::Super;
	using Super::operator=;

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
	using ValueType = TOptional<InValueType>;
	using ValidType = InValueType;

	using Super::Super;
	using Super::operator=;
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


template <typename Derived, typename InValueType>
class TBackedLiveDataBase : public TLiveDataBase<Derived, InValueType&>
{
public:
	using Super = TLiveDataBase<Derived, InValueType&>;

	using Super::Super;

	template <typename T>
		requires CInequalityComparable<InValueType> && std::is_assignable_v<InValueType&, T>
	TBackedLiveDataBase& operator=(T&& NewValue)
	{
		if constexpr (std::is_same_v<InValueType&, T>)
		{
			check(&Super::Get() != &NewValue);
		}
		
		Super::operator=(Forward<T>(NewValue));
		return *this;
	}
};


enum class ERepHandlingPolicy
{
	AlwaysNotify,
	CompareForAddOrRemove,
};


template <typename ValueType, ERepHandlingPolicy = ERepHandlingPolicy::AlwaysNotify>
class TBackedLiveData;


template <typename InValueType>
class TBackedLiveData<InValueType, ERepHandlingPolicy::AlwaysNotify>
	: private TBackedLiveDataBase<TBackedLiveData<InValueType>, InValueType>
{
public:
	using Super = TBackedLiveDataBase<TBackedLiveData, InValueType>;
	using ValueType = InValueType;
	using ValidType = InValueType;

	using Super::Super;
	using Super::operator=;
	using Super::SetValue;
	using Super::SetValueSilent;
	using Super::SetValueAlways;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	ValidType* operator->() const { return Super::Get(); }
	const ValidType& operator*() const { return *Super::Get(); }

	void OnRep() { Notify(); }

private:
	friend class Super::Super;
	static bool IsValid(const ValueType&) { return true; }
	static const ValidType& ToValid(const ValueType& Value [[msvc::lifetimebound]]) { return Value; }
};


template <typename InValueType>
class TBackedLiveData<InValueType*, ERepHandlingPolicy::AlwaysNotify>
	: private TBackedLiveDataBase<TBackedLiveData<InValueType*>, InValueType*>
{
public:
	using Super = TBackedLiveDataBase<TBackedLiveData, InValueType*>;
	using ValueType = InValueType*;
	using ValidType = InValueType*;

	using Super::Super;
	using Super::operator=;
	using Super::SetValue;
	using Super::SetValueSilent;
	using Super::SetValueAlways;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	ValidType operator->() const { return Super::Get(); }
	const InValueType& operator*() const { return *Super::Get(); }

	void OnRep() { Notify(); }
	
	friend TCancellableFutureAwaitable<ValidType> operator co_await(TBackedLiveData& LiveData)
	{
		return [&]() -> TCancellableFuture<ValidType>
		{
			if (TBackedLiveData::IsValid(LiveData.Get()))
			{
				return TBackedLiveData::ToValid(LiveData.Get());
			}

			return MakeFutureFromDelegate<ValidType>(LiveData.OnChanged,
				[](const ValueType& Value) { return TBackedLiveData::IsValid(Value); },
				[](const ValueType& Value) { return TBackedLiveData::ToValid(Value); });
		}();
	}

private:
	friend class Super::Super;
	static bool IsValid(const ValueType& Value) { return ::IsValid(Value); }
	static ValidType ToValid(const ValueType& Value) { return Value; }
};


template <typename InValueType>
class TBackedLiveData<TScriptInterface<InValueType>, ERepHandlingPolicy::AlwaysNotify>
	: private TBackedLiveDataBase<TBackedLiveData<TScriptInterface<InValueType>>, TScriptInterface<InValueType>>
{
public:
	using Super = TBackedLiveDataBase<TBackedLiveData, TScriptInterface<InValueType>>;
	using ValueType = TScriptInterface<InValueType>;
	using ValidType = TScriptInterface<InValueType>;

	using Super::Super;
	using Super::operator=;
	using Super::SetValue;
	using Super::SetValueSilent;
	using Super::SetValueAlways;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	InValueType* operator->() const { return Super::Get().GetInterface(); }
	const InValueType& operator*() const { return *Super::Get().GetInterface(); }

	void OnRep() { Notify(); }
	
	friend TCancellableFutureAwaitable<ValidType> operator co_await(TBackedLiveData& LiveData)
	{
		return [&]() -> TCancellableFuture<ValidType>
		{
			if (TBackedLiveData::IsValid(LiveData.Get()))
			{
				return TBackedLiveData::ToValid(LiveData.Get());
			}

			return MakeFutureFromDelegate<ValidType>(LiveData.OnChanged,
				[](const ValueType& Value) { return TBackedLiveData::IsValid(Value); },
				[](const ValueType& Value) { return TBackedLiveData::ToValid(Value); });
		}();
	}

	InValueType* GetInterface() const { return Super::Get().GetInterface(); }

private:
	friend class Super::Super;
	static bool IsValid(const ValueType& Value) { return ::IsValid(Value.GetObject()); }
	static ValidType ToValid(const ValueType& Value) { return Value; }
};


template <typename ElementType>
class TBackedLiveData<TArray<ElementType>, ERepHandlingPolicy::CompareForAddOrRemove>
	: public TBackedLiveDataBase<TBackedLiveData<TArray<ElementType>>, TArray<ElementType>>
{
public:
	using Super = TBackedLiveDataBase<TBackedLiveData<TArray<ElementType>>, TArray<ElementType>>;
	using ValueType = TArray<ElementType>;
	using ValidType = TArray<ElementType>;

	using Super::Super;
	using Super::operator=;
	using Super::Notify;
	using Super::Observe;
	using Super::CreateStream;
	using Super::Get;

	ValidType* operator->() const { return Super::Get(); }
	const ValidType& operator*() const { return *Super::Get(); }

	template <typename U>
	void Add(U&& Element) requires std::is_convertible_v<U, ElementType>
	{
		this->Value.Add(Forward<U>(Element));
		NotifyAdd(this->Value.Last());
	}

	void Remove(const ElementType& Element)
	{
		this->Value.Remove(Element);
		NotifyRemove(Element);
	}

	void RemoveAt(int32 Index)
	{
		auto Removed = MoveTemp(this->Value[Index]);
		this->Value.RemoveAt(Index);
		NotifyRemove(Removed);
	}

	void Empty()
	{
		auto Removed = MoveTemp(this->Value);
		this->Value.Empty();
		for (const ElementType& Each : Removed)
		{
			this->GuardCallbackInvocation([&]() { OnElementRemoved.Broadcast(Each); });
		}
		Super::Notify();
	}

	void OnRep(const TArray<ElementType>& OldArray)
	{
		for (const ElementType& Each : OldArray)
		{
			if (!this->Value.Contains(Each))
			{
				this->GuardCallbackInvocation([&]() { OnElementRemoved.Broadcast(Each); });
			}
		}

		for (const ElementType& Each : this->Value)
		{
			if (!OldArray.Contains(Each))
			{
				this->GuardCallbackInvocation([&]() { OnElementAdded.Broadcast(Each); });
			}
		}

		Super::Notify();
	}

	void NotifyAdd(const ElementType& Element)
	{
		this->GuardCallbackInvocation([&]() { OnElementAdded.Broadcast(Element); });
		Super::Notify();
	}

	void NotifyRemove(const ElementType& Element)
	{
		this->GuardCallbackInvocation([&]() { OnElementRemoved.Broadcast(Element); });
		Super::Notify();
	}

	template <typename FuncType>
	void ObserveAdd(UObject* Lifetime, FuncType&& Func)
	{
		for (const ElementType& Each : this->Value)
		{
			this->GuardCallbackInvocation([&]() { Func(Each); });
		}

		OnElementAdded.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void ObserveAdd(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		for (const ElementType& Each : this->Value)
		{
			this->GuardCallbackInvocation([&]() { Func(Each); });
		}

		OnElementAdded.Add(FElementEvent::FDelegate::CreateSPLambda(Lifetime, Forward<FuncType>(Func)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle ObserveAdd(FuncType&& Func)
	{
		FDelegateSPHandle Ret;
		ObserveAdd(Ret.ToShared(), Forward<FuncType>(Func));
		return Ret;
	}

	template <typename FuncType>
	void ObserveRemove(UObject* Lifetime, FuncType&& Func)
	{
		OnElementRemoved.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void ObserveRemove(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		OnElementRemoved.Add(FElementEvent::FDelegate::CreateSPLambda(Lifetime, Forward<FuncType>(Func)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle ObserveRemove(FuncType&& Func)
	{
		FDelegateSPHandle Ret;
		ObserveRemove(Ret.ToShared(), Forward<FuncType>(Func));
		return Ret;
	}

	TValueStream<ElementType> CreateAddStream()
	{
		return CreateMulticastValueStream(this->Value, OnElementAdded);
	}

private:
	friend class Super::Super;
	static bool IsValid(const ValueType&) { return true; }
	static const ValidType& ToValid(const ValueType& Value [[msvc::lifetimebound]]) { return Value; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FElementEvent, const ElementType&);
	FElementEvent OnElementAdded;
	FElementEvent OnElementRemoved;
};


template <typename LiveDataType>
class TLiveDataView
{
public:
	using ValidType = typename LiveDataType::ValidType;

	TLiveDataView(LiveDataType& InLiveData)
		: LiveData(InLiveData)
	{
	}

	template <typename... ArgTypes>
	decltype(auto) Observe(ArgTypes&&... Args) { return LiveData.Observe(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveAdd(ArgTypes&&... Args) { return LiveData.ObserveAdd(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveRemove(ArgTypes&&... Args) { return LiveData.ObserveRemove(Forward<ArgTypes>(Args)...); }

	decltype(auto) CreateStream() { return LiveData.CreateStream(); }
	decltype(auto) CreateAddStream() { return LiveData.CreateAddStream(); }

	template <CEqualityComparable<ValidType> ArgType>
	TCancellableFuture<ValidType, EValueStreamError> If(ArgType&& OfThis)
	{
		return FirstInStream(CreateStream(), [OfThis = Forward<ArgType>(OfThis)](const ValidType& Value) { return Value == OfThis; });
	}

	template <CPredicate<ValidType> PredicateType>
	TCancellableFuture<ValidType, EValueStreamError> If(PredicateType&& Predicate)
	{
		return FirstInStream(CreateStream(), [Predicate = Forward<PredicateType>(Predicate)](const ValidType& Value) { return Predicate(Value); });
	}

	friend TCancellableFutureAwaitable<ValidType> operator co_await(TLiveDataView LiveDataView)
	{
		return operator co_await(LiveDataView.LiveData);
	}

	decltype(auto) Get() { return LiveData.Get(); }
	decltype(auto) Get() const { return LiveData.Get(); }
	decltype(auto) operator*() { return *LiveData; }
	decltype(auto) operator*() const { return *LiveData; }
	decltype(auto) operator->() { return LiveData.operator->(); }
	decltype(auto) operator->() const { return LiveData.operator->(); }

private:
	LiveDataType& LiveData;
};


template <typename LiveDataType>
TLiveDataView<LiveDataType> ToLiveDataView(LiveDataType& LiveData)
{
	return TLiveDataView<LiveDataType>{LiveData};
}


// TLiveData Declarations
#define DECLARE_REPPED_LIVE_DATA_GETTER_SETTER(Type, Name, BackingField)\
	private: TBackedLiveData<Type> Name{BackingField};\
	public:\
	TLiveDataView<TBackedLiveData<Type>> Get##Name() { return Name; };\
	void Set##Name(const std::type_identity_t<Type>& NewValue) { DEFINE_REPPED_VAR_SETTER(Name, NewValue); }
// ~TLiveData Declarations
