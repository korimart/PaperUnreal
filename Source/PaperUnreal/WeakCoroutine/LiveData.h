// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"
#include "ValueStream.h"


template <typename ValueType>
struct TLiveDataValidator
{
	static constexpr bool bSupported = false;
};


template <typename OptionalType>
struct TLiveDataValidator<TOptional<OptionalType>>
{
	static constexpr bool bSupported = true;

	using ValidType = OptionalType;

	static bool IsValid(const TOptional<OptionalType>& Value) { return Value.IsSet(); }

	// LiveData 안에 원본이 살아있을 테니 life time bound를 가정하고 ref를 반환하라는 뜻
	static const OptionalType& ToValidRef(const TOptional<OptionalType>& Value UE_LIFETIMEBOUND) { return Value.GetValue(); }

	// LiveData 안에 원본이 살아있지 않을 테니 copy 또는 move를 사용해 새로 만들라는 뜻
	static OptionalType ToValidConstruct(const TOptional<OptionalType>& Value) { return Value.GetValue(); }
	static OptionalType ToValidConstruct(TOptional<OptionalType>&& Value) { return MoveTemp(Value.GetValue()); }
};


template <typename ValueType>
struct TLiveDataValidator<ValueType*>
{
	static constexpr bool bSupported = true;

	using ValidType = ValueType*;

	static bool IsValid(ValueType* Value) { return ::IsValid(Value); }

	// LiveData 안에 원본이 살아있을 테니 life time bound를 가정하고 ref를 반환하라는 뜻
	static ValueType* ToValidRef(ValueType* Value) { return Value; }

	// LiveData 안에 원본이 살아있지 않을 테니 copy 또는 move를 사용해 새로 만들라는 뜻
	static ValueType* ToValidConstruct(ValueType* Value) { return Value; }
};


class FLiveDataBase
{
protected:
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

	template <typename Validator, typename FuncType>
	static auto RelayValidRefTo(FuncType Func)
	{
		return [Func = Forward<FuncType>(Func)](const auto& Value)
		{
			if (Validator::IsValid(Value))
			{
				Func(Validator::ToValidRef(Value));
			}
		};
	}
};


template <typename ValueType, typename = void>
class TLiveData2 : public FLiveDataBase
{
public:
	using DecayedValueType = std::decay_t<ValueType>;
	using ConstRefValueType = const DecayedValueType&;
	using Validator = TLiveDataValidator<DecayedValueType>;

	TLiveData2() requires std::is_default_constructible_v<ValueType>
		: Value{}
	{
	}

	template <typename T> requires std::is_convertible_v<ValueType, T>
	explicit TLiveData2(T&& Init)
		: Value(Forward<T>(Init))
	{
	}

	template <typename T> requires CInequalityComparable<ValueType, T> && std::is_assignable_v<ValueType&, T>
	TLiveData2& operator=(T&& NewValue)
	{
		SetValue(Forward<T>(NewValue));
		return *this;
	}

	template <typename T> requires CInequalityComparable<ValueType, T> && std::is_assignable_v<ValueType&, T>
	void SetValue(T&& Right)
	{
		if (SetValueSilent(Forward<T>(Right)))
		{
			Notify();
		}
	}

	template <typename T> requires CInequalityComparable<ValueType, T> && std::is_assignable_v<ValueType&, T>
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

	template <typename T> requires std::is_assignable_v<ValueType&, T>
	void SetValueNoComparison(T&& Right)
	{
		Value = Forward<T>(Right);
		Notify();
	}

	template <typename FuncType>
	void Modify(const FuncType& Func)
	{
		// TODO static assert Func takes a non-const lvalue reference
		Func(Value);
		Notify();
	}

	void Notify()
	{
		GuardCallbackInvocation([&]() { OnChanged.Broadcast(Get()); });
	}

	void OnRep()
	{
		Notify();
	}
	
	template <typename T, typename FuncType>
	void Observe(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		Func(Get());
		OnChanged.Add(FOnChanged::FDelegate::CreateSPLambda(Lifetime, Forward<FuncType>(Func)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle Observe(FuncType&& Func)
	{
		FDelegateSPHandle Ret;
		Observe(Ret.ToShared(), Forward<FuncType>(Func));
		return Ret;
	}
	
	template <typename T, typename FuncType>
	void ObserveIfValid(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		auto ValidCheckingFunc = RelayValidRefTo<Validator>(Forward<FuncType>(Func));
		ValidCheckingFunc(Get());
		OnChanged.Add(FOnChanged::FDelegate::CreateSPLambda(Lifetime, MoveTemp(ValidCheckingFunc)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle ObserveIfValid(FuncType&& Func) requires Validator::bSupported
	{
		FDelegateSPHandle Ret;
		ObserveIfValid(Ret.ToShared(), Forward<FuncType>(Func));
		return Ret;
	}

	ConstRefValueType Get() const
	{
		return Value;
	}

	bool IsValid() const requires Validator::bSupported
	{
		return Validator::IsValid(Get());
	}

	decltype(auto) GetValid() const UE_LIFETIMEBOUND requires Validator::bSupported
	{
		return Validator::ToValidRef(Get());
	}

	friend auto operator co_await(TLiveData2& LiveData) requires Validator::bSupported
	{
		return TCancellableFutureAwaitable<typename Validator::ValidType>
		{
			[&]() -> TCancellableFuture<typename Validator::ValidType>
			{
				if (LiveData.IsValid())
				{
					return LiveData.GetValid();
				}

				return MakeFutureFromDelegate(LiveData.OnChanged,
					[](ConstRefValueType NewValue) { return Validator::IsValid(NewValue); },
					[](ConstRefValueType NewValue) { return Validator::ToValidConstruct(NewValue); });
			}()
		};
	}

	TValueStream<DecayedValueType> CreateStream()
	{
		return CreateMulticastValueStream(TArray<DecayedValueType>{Value}, OnChanged);
	}

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, ConstRefValueType);

	ValueType Value;
	FOnChanged OnChanged;
};


template <typename ArrayType>
class TLiveData2<
		ArrayType,
		std::void_t<std::enable_if_t<TIsInstantiationOf_V<std::decay_t<ArrayType>, TArray>>>
	> : public FLiveDataBase
{
public:
	using ElementType = typename std::decay_t<ArrayType>::ElementType;
	using Validator = TLiveDataValidator<ElementType>;

	TLiveData2() requires std::is_default_constructible_v<ArrayType>
		: Array{}
	{
	}

	template <typename T> requires std::is_convertible_v<T, ArrayType>
	explicit TLiveData2(T&& Init)
		: Array(Forward<T>(Init))
	{
	}

	TLiveData2(const TLiveData2&) = delete;
	TLiveData2 operator=(const TLiveData2&) = delete;

	template <typename T>
	void Add(T&& Element) requires std::is_convertible_v<T, ElementType>
	{
		Array.Add(Forward<T>(Element));
		NotifyAdd(Array.Last());
	}

	template <typename T>
	void AddIfValid(T&& Element) requires Validator::bSupported && std::is_convertible_v<T, ElementType>
	{
		if (Validator::IsValid(Element))
		{
			Array.Add(Forward<T>(Element));
			NotifyAdd(Array.Last());
		}
	}

	void Remove(const ElementType& Element)
	{
		Array.Remove(Element);
		NotifyRemove(Element);
	}

	void RemoveAt(int32 Index)
	{
		auto Removed = MoveTemp(Array[Index]);
		Array.RemoveAt(Index);
		NotifyRemove(Removed);
	}

	void Empty()
	{
		// OnElementRemoved의 콜백에서 Array가 비었음을 즉시 볼 수 있도록 미리 비움
		auto Removed = MoveTemp(Array);
		for (const ElementType& Each : Removed)
		{
			NotifyRemove(Each);
		}
	}

	void NotifyDiff(const TArray<ElementType>& OldArray)
	{
		for (const ElementType& Each : OldArray)
		{
			if (!Array.Contains(Each))
			{
				NotifyRemove(Each);
			}
		}

		for (const ElementType& Each : Array)
		{
			if (!OldArray.Contains(Each))
			{
				NotifyAdd(Each);
			}
		}
	}

	template <typename FuncType>
	void ObserveAdd(UObject* Lifetime, FuncType&& Func)
	{
		for (const ElementType& Each : Array)
		{
			this->GuardCallbackInvocation([&]() { Func(Each); });
		}

		OnElementAdded.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void ObserveAdd(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		for (const ElementType& Each : Array)
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
	void ObserveAddIfValid(const auto Lifetime, FuncType&& Func) requires Validator::bSupported
	{
		ObserveAdd(Lifetime, RelayValidRefTo<Validator>(Forward<FuncType>(Func)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle ObserveAddIfValid(FuncType&& Func) requires Validator::bSupported
	{
		return ObserveAdd(RelayValidRefTo<Validator>(Forward<FuncType>(Func)));
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

	template <typename FuncType>
	void ObserveRemoveIfValid(const auto Lifetime, FuncType&& Func) requires Validator::bSupported
	{
		ObserveRemove(Lifetime, RelayValidRefTo<Validator>(Forward<FuncType>(Func)));
	}

	template <typename FuncType>
	UE_NODISCARD FDelegateSPHandle ObserveRemoveIfValid(FuncType&& Func) requires Validator::bSupported
	{
		return ObserveRemove(RelayValidRefTo<Validator>(Forward<FuncType>(Func)));
	}

	TValueStream<ElementType> CreateAddStream()
	{
		return CreateMulticastValueStream(Array, OnElementAdded);
	}

private:
	ArrayType Array;

	DECLARE_MULTICAST_DELEGATE_OneParam(FElementEvent, const ElementType&);
	FElementEvent OnElementAdded;
	FElementEvent OnElementRemoved;

	void NotifyAdd(const ElementType& Element)
	{
		this->GuardCallbackInvocation([&]() { OnElementAdded.Broadcast(Element); });
	}

	void NotifyRemove(const ElementType& Element)
	{
		this->GuardCallbackInvocation([&]() { OnElementRemoved.Broadcast(Element); });
	}
};


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

	template <typename T> requires std::is_assignable_v<InternalStorageType&, T>
	TLiveDataBase& operator=(T&& NewValue)
	{
		SetValue(Forward<T>(NewValue));
		return *this;
	}

	template <typename T> requires std::is_assignable_v<InternalStorageType&, T>
	void SetValue(T&& Right)
	{
		check(!bExecutingCallbacks);
		if (SetValueSilent(Forward<T>(Right)))
		{
			Notify();
		}
	}

	template <typename T> requires std::is_assignable_v<InternalStorageType&, T>
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

	template <typename T> requires std::is_assignable_v<InValueType&, T>
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
