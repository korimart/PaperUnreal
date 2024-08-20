﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TypeTraits.h"
#include "FilterAwaitable.h"
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


template <typename ValueType>
struct TLiveDataValidator<TObjectPtr<ValueType>> : TLiveDataValidator<ValueType*>
{
};


template <typename InterfaceType>
struct TLiveDataValidator<TScriptInterface<InterfaceType>>
{
	static constexpr bool bSupported = true;

	using ValidType = InterfaceType*;

	static bool IsValid(const TScriptInterface<InterfaceType>& Value) { return ::IsValid(Value.GetObject()); }

	// LiveData 안에 원본이 살아있을 테니 life time bound를 가정하고 ref를 반환하라는 뜻
	static InterfaceType* ToValidRef(const TScriptInterface<InterfaceType>& Value) { return Value.GetInterface(); }

	// LiveData 안에 원본이 살아있지 않을 테니 copy 또는 move를 사용해 새로 만들라는 뜻
	static InterfaceType* ToValidConstruct(const TScriptInterface<InterfaceType>& Value) { return Value.GetInterface(); }
};


template <typename ObjectType>
struct TLiveDataValidator<TSoftObjectPtr<ObjectType>>
{
	static constexpr bool bSupported = true;

	using ValidType = TSoftObjectPtr<ObjectType>;

	static bool IsValid(const TSoftObjectPtr<ObjectType>& Value) { return !Value.IsNull(); }

	// LiveData 안에 원본이 살아있을 테니 life time bound를 가정하고 ref를 반환하라는 뜻
	static const TSoftObjectPtr<ObjectType>& ToValidRef(const TSoftObjectPtr<ObjectType>& Value UE_LIFETIMEBOUND) { return Value; }

	// LiveData 안에 원본이 살아있지 않을 테니 copy 또는 move를 사용해 새로 만들라는 뜻
	static TSoftObjectPtr<ObjectType> ToValidConstruct(const TSoftObjectPtr<ObjectType>& Value) { return Value; }
};


class FLiveDataBase
{
public:
	FLiveDataBase() = default;

	// Value Stream을 Multicast Delegate을 통해서 만들기 때문에 허용하면 여러 곳에서
	// Value Stream을 Value를 넣을 수 있게 됨
	FLiveDataBase(const FLiveDataBase&) = delete;
	FLiveDataBase& operator=(const FLiveDataBase&) = delete;

	FLiveDataBase(FLiveDataBase&&) = default;

protected:
	bool bExecutingCallbacks = false;
	TArray<TCancellablePromise<void>> CallbackGuardAwaiters;

	struct FScopedCallbackGuard
	{
		FScopedCallbackGuard(FLiveDataBase* InThis)
			: This(InThis)
		{
			// 여기 걸리면 콜백 호출 도중에 LiveData를 수정하려고 한 것임
			// Array 순회 도중에 Array를 수정하려고 한 것과 비슷함
			check(!This->bExecutingCallbacks);
			This->bExecutingCallbacks = true;
		}

		~FScopedCallbackGuard()
		{
			LetAwaitersIn();
			This->bExecutingCallbacks = false;
		}

	private:
		FLiveDataBase* This;

		void LetAwaitersIn()
		{
			while (This->CallbackGuardAwaiters.Num() > 0)
			{
				PopFront(This->CallbackGuardAwaiters).SetValue();
			}
		}
	};

	template <typename FuncType>
	void AwaitCallbackGuard(FuncType&& Func)
	{
		if (!bExecutingCallbacks)
		{
			FScopedCallbackGuard S{this};
			Func();
			return;
		}

		auto [Promise, Future] = MakePromise<void>();

		Future.Then([Func = Forward<FuncType>(Func)](const auto& Result) mutable
		{
			if (Result.Succeeded())
			{
				Func();
			}
		});

		CallbackGuardAwaiters.Add(MoveTemp(Promise));
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


template <typename InValueType, typename = void>
class TLiveData : public FLiveDataBase
{
public:
	using ValueType = InValueType;
	using DecayedValueType = std::decay_t<ValueType>;
	using ConstRefValueType = const DecayedValueType&;
	using Validator = TLiveDataValidator<DecayedValueType>;

	TLiveData() requires std::is_default_constructible_v<ValueType>
		: Value{}
	{
	}

	template <typename T> requires std::is_convertible_v<ValueType, T>
	explicit TLiveData(T&& Init)
		: Value(Forward<T>(Init))
	{
	}

	template <typename T> requires CInequalityComparable<ValueType, T> && std::is_assignable_v<ValueType&, T>
	TLiveData& operator=(T&& NewValue)
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
		if (Value != Right)
		{
			check(!bExecutingCallbacks);
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

	void Modify(const auto& Func)
	{
		FScopedCallbackGuard S{this};

		// TODO static assert Func takes a non-const lvalue reference
		if (Func(Value))
		{
			OnChanged.Broadcast(Get());
		}
	}

	template <typename FuncType>
	void AwaitAndModify(FuncType&& Func)
	{
		AwaitCallbackGuard([this, Func = Forward<FuncType>(Func)]() mutable
		{
			if (Func(Value))
			{
				OnChanged.Broadcast(Get());
			}
		});
	}

	void Notify()
	{
		FScopedCallbackGuard S{this};
		OnChanged.Broadcast(Get());
	}

	template <typename FuncType>
	void Observe(UObject* Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};
		Func(Get());
		OnChanged.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void Observe(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};
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

	template <typename FuncType>
	void ObserveIfValid(UObject* Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};
		auto ValidCheckingFunc = RelayValidRefTo<Validator>(Forward<FuncType>(Func));
		ValidCheckingFunc(Get());
		OnChanged.AddWeakLambda(Lifetime, MoveTemp(ValidCheckingFunc));
	}

	template <typename T, typename FuncType>
	void ObserveIfValid(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};
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

	ConstRefValueType operator->() const requires std::is_pointer_v<DecayedValueType>
	{
		return Get();
	}

	bool IsValid() const requires Validator::bSupported
	{
		return Validator::IsValid(Get());
	}

	decltype(auto) GetValid() const UE_LIFETIMEBOUND requires Validator::bSupported
	{
		return Validator::ToValidRef(Get());
	}

	friend auto operator co_await(TLiveData& LiveData) requires Validator::bSupported
	{
		return TCancellableFutureAwaitable<TCancellableFuture<typename Validator::ValidType>>
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

	TValueStream<DecayedValueType> MakeStream()
	{
		auto Ret = MakeStreamFromDelegate(OnChanged);
		Ret.GetReceiver().Pin()->ReceiveValue(Value);
		return Ret;
	}

private:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, ConstRefValueType);

	ValueType Value;
	FOnChanged OnChanged;
};


template <typename ArrayType>
class TLiveData<
		ArrayType,
		std::void_t<std::enable_if_t<TIsInstantiationOf_V<std::decay_t<ArrayType>, TArray>>>
	> : public FLiveDataBase
{
public:
	using ElementType = typename std::decay_t<ArrayType>::ElementType;
	using Validator = TLiveDataValidator<ElementType>;

	TLiveData() requires std::is_default_constructible_v<ArrayType>
		: Array{}
	{
	}

	template <typename T> requires std::is_convertible_v<T, ArrayType>
	explicit TLiveData(T&& Init)
		: Array(Forward<T>(Init))
	{
	}

	TLiveData(const TLiveData&) = delete;
	TLiveData operator=(const TLiveData&) = delete;

	template <typename T>
	void SetValueSilent(T&& NewArray)
	{
		check(!bExecutingCallbacks);
		Array = Forward<T>(NewArray);
	}

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
		FScopedCallbackGuard S{this};

		// OnElementRemoved의 콜백에서 Array가 비었음을 즉시 볼 수 있도록 미리 비움
		auto Removed = MoveTemp(Array);
		for (const ElementType& Each : Removed)
		{
			OnElementRemoved.Broadcast(Each);
			OnArrayChanged.Broadcast(Get());
		}

		if (Removed.Num() > 0)
		{
			CloseStrictAddStreams();
		}
	}

	void NotifyDiff(const TArray<ElementType>& OldArray)
	{
		FScopedCallbackGuard S{this};

		bool bAnyRemoved = false;
		for (const ElementType& Each : OldArray)
		{
			if (!Array.Contains(Each))
			{
				OnElementRemoved.Broadcast(Each);
				OnArrayChanged.Broadcast(Get());
				bAnyRemoved = true;
			}
		}

		for (const ElementType& Each : Array)
		{
			if (!OldArray.Contains(Each))
			{
				OnElementAdded.Broadcast(Each);
				OnArrayChanged.Broadcast(Get());
			}
		}

		if (bAnyRemoved)
		{
			CloseStrictAddStreams();
		}
	}

	template <typename FuncType>
	void ObserveAdd(UObject* Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};

		for (const ElementType& Each : Array)
		{
			Func(Each);
		}

		OnElementAdded.AddWeakLambda(Lifetime, Forward<FuncType>(Func));
	}

	template <typename T, typename FuncType>
	void ObserveAdd(const TSharedRef<T>& Lifetime, FuncType&& Func)
	{
		FScopedCallbackGuard S{this};

		for (const ElementType& Each : Array)
		{
			Func(Each);
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
	void ObserveAddIfValid(const auto& Lifetime, FuncType&& Func) requires Validator::bSupported
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

	TValueStream<std::decay_t<ArrayType>> MakeStream()
	{
		auto Ret = MakeStreamFromDelegate(OnArrayChanged);
		Ret.GetReceiver().Pin()->ReceiveValue(Array);
		return Ret;
	}

	TValueStream<ElementType> MakeAddStream()
	{
		auto Ret = MakeStreamFromDelegate(OnElementAdded);
		Ret.GetReceiver().Pin()->ReceiveValues(Array);
		return Ret;
	}

	TValueStream<ElementType> MakeStrictAddStream()
	{
		FDelegateHandle Handle;
		auto Ret = MakeStreamFromDelegate(OnElementAdded, &Handle);
		Ret.GetReceiver().Pin()->ReceiveValues(Array);
		StrictAddStreamHandles.Add(Handle);
		return Ret;
	}

	TCancellableFuture<void> WaitForElementToBeRemoved(const ElementType& Element)
	{
		return MakeFutureFromDelegate(
			OnElementRemoved,
			[Element](const ElementType& Removed) { return Removed == Element; },
			[](const ElementType& Removed) { return; });
	}

	const ArrayType& Get() const { return Array; }

private:
	ArrayType Array;

	DECLARE_MULTICAST_DELEGATE_OneParam(FArrayEvent, const ArrayType&);
	FArrayEvent OnArrayChanged;

	DECLARE_MULTICAST_DELEGATE_OneParam(FElementEvent, const ElementType&);
	FElementEvent OnElementAdded;
	FElementEvent OnElementRemoved;

	TArray<FDelegateHandle> StrictAddStreamHandles;

	void CloseStrictAddStreams()
	{
		// 델리게이트를 unbind하면 델리게이트가 파괴되면서 stream을 종료함
		// 그 때 콜백으로 다시 MakeStrictAddStream이 호출될 수 있기 때문에 Array가 순회 도중 수정되는 것을
		// 피하기 위해 먼저 비운다음에 델리게이트를 unbind 한다
		auto Copy = MoveTemp(StrictAddStreamHandles);
		for (FDelegateHandle Each : Copy)
		{
			OnElementAdded.Remove(Each);
		}
	}

	void NotifyAdd(const ElementType& Element)
	{
		FScopedCallbackGuard S{this};
		OnElementAdded.Broadcast(Element);
		OnArrayChanged.Broadcast(Get());
	}

	void NotifyRemove(const ElementType& Element)
	{
		FScopedCallbackGuard S{this};
		OnElementRemoved.Broadcast(Element);
		OnArrayChanged.Broadcast(Get());
		CloseStrictAddStreams();
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

	//
	// Forwarding Functions
	//

	template <typename... ArgTypes>
	decltype(auto) Observe(ArgTypes&&... Args) { return LiveData.Observe(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveIfValid(ArgTypes&&... Args) { return LiveData.ObserveIfValid(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveAdd(ArgTypes&&... Args) { return LiveData.ObserveAdd(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveAddIfValid(ArgTypes&&... Args) { return LiveData.ObserveAddIfValid(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveRemove(ArgTypes&&... Args) { return LiveData.ObserveRemove(Forward<ArgTypes>(Args)...); }

	template <typename... ArgTypes>
	decltype(auto) ObserveRemoveIfValid(ArgTypes&&... Args) { return LiveData.ObserveRemoveIfValid(Forward<ArgTypes>(Args)...); }

	decltype(auto) MakeStream() { return LiveData.MakeStream(); }
	decltype(auto) MakeAddStream() { return LiveData.MakeAddStream(); }
	decltype(auto) MakeStrictAddStream() { return LiveData.MakeStrictAddStream(); }

	decltype(auto) WaitForElementToBeRemoved(const auto& Element) { return LiveData.WaitForElementToBeRemoved(Element); }

	friend decltype(auto) operator co_await(TLiveDataView LiveDataView) { return operator co_await(LiveDataView.LiveData); }

	decltype(auto) Get() { return LiveData.Get(); }
	decltype(auto) Get() const { return LiveData.Get(); }

	decltype(auto) operator->() const { return LiveData.operator->(); }
	decltype(auto) operator->() { return LiveData.operator->(); }

	//
	// ~Forwarding Functions
	//

	//
	// Live Data View Convenience Functions
	//

	template <typename ArgType>
	auto If(ArgType&& OfThis)
	{
		return MakeStream() | Awaitables::If(Forward<ArgType>(OfThis));
	}

	template <typename ArgType>
	auto IfNot(ArgType&& OfThis)
	{
		return MakeStream() | Awaitables::IfNot(Forward<ArgType>(OfThis));
	}

	//
	// ~Live Data View Convenience Functions
	//

private:
	TLiveData<ValueType>& LiveData;
};


#define DECLARE_LIVE_DATA_GETTER_SETTER(Name)\
	template <typename U>\
	void Set##Name(U&& NewValue){ check(GetNetMode() != NM_Client); Name = Forward<U>(NewValue); }\
	auto Get##Name() const { return TLiveDataView{Name}; }
