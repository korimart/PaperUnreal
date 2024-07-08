﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>

#include "Algo/AllOf.h"


template <typename...>
struct TFalse
{
	static constexpr bool Value = false;
};


template <typename, template <typename...> typename>
struct TIsInstantiationOf : std::false_type
{
};

template <typename... Args, template <typename...> typename T>
struct TIsInstantiationOf<T<Args...>, T> : std::true_type
{
};

template <typename T, template <typename...> typename Template>
inline constexpr bool TIsInstantiationOf_V = TIsInstantiationOf<T, Template>::value;


template <typename SignatureType>
struct TGetFirstParam;

template <typename RetType, typename First, typename... Rest>
struct TGetFirstParam<RetType(First, Rest...)>
{
	using Type = First;
};


// TODO 위에 거랑 합치기
template <typename FunctorType>
struct TGetFirstParam2
{
private:
	using FuncType = typename TDecay<FunctorType>::Type;

	template <typename F, typename Ret>
	static void GetFirstParamHelper(Ret (F::*)());

	template <typename F, typename Ret>
	static void GetFirstParamHelper(Ret (F::*)() const);

	template <typename F, typename Ret, typename A, typename... Rest>
	static A GetFirstParamHelper(Ret (F::*)(A, Rest...));

	template <typename F, typename Ret, typename A, typename... Rest>
	static A GetFirstParamHelper(Ret (F::*)(A, Rest...) const);

public:
	using Type = decltype(GetFirstParamHelper(&FuncType::operator()));
};


class FWeakCoroutineContext;

template <typename>
class TWeakAwaitable;

template <typename>
class TReadyWeakAwaitable;


struct FWeakCoroutine
{
	struct promise_type
	{
		TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures;
		bool bCustomSuspended = false;
		TSharedPtr<bool> bDestroyed = MakeShared<bool>(false);

		bool IsValid() const
		{
			return Algo::AllOf(WeakList, [](const auto& Each) { return Each(); });
		}

		void AddToWeakList(UObject* Object)
		{
			WeakList.Add([Weak = TWeakObjectPtr{Object}]() { return Weak.IsValid(); });
		}

		template <typename T>
		void AddToWeakList(const TSharedPtr<T>& Object)
		{
			WeakList.Add([Weak = TWeakPtr<T>{Object}]() { return Weak.IsValid(); });
		}
		
		template <typename T>
		void AddToWeakList(const TSharedRef<T>& Object)
		{
			WeakList.Add([Weak = TWeakPtr<T>{Object}]() { return Weak.IsValid(); });
		}

		template <typename T>
		void AddToWeakList(const TWeakPtr<T>& Object)
		{
			WeakList.Add([Weak = Object]() { return Weak.IsValid(); });
		}

		FWeakCoroutine get_return_object()
		{
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}

		std::suspend_always initial_suspend() const
		{
			return {};
		}

		std::suspend_never final_suspend() const noexcept
		{
			*bDestroyed = true;
			return {};
		}

		void return_void() const
		{
		}

		void unhandled_exception() const
		{
		}

		template <typename AwaitableType>
		decltype(auto) await_transform(AwaitableType&& Awaitable)
		{
			if constexpr (
				TIsInstantiationOf_V<std::decay_t<AwaitableType>, TReadyWeakAwaitable>
				|| TIsInstantiationOf_V<std::decay_t<AwaitableType>, TWeakAwaitable>)
			{
				return Forward<AwaitableType>(Awaitable);
			}
			else
			{
				static_assert(TFalse<AwaitableType>::Value);
			}
		}

	private:
		TArray<TFunction<bool()>> WeakList;
	};

	void Init(
		auto& Lifetime,
		TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
		TUniquePtr<FWeakCoroutineContext> Context);

private:
	std::coroutine_handle<promise_type> Handle;

	FWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle)
	{
	}
};

class FWeakCoroutineContext
{
public:
	template <typename T>
	void AddToWeakList(T&& Weak)
	{
		Handle.promise().AddToWeakList(Forward<T>(Weak));
	}

	// TODO 필요해지면 구현
	template <typename T>
	void DiableAutoAddOfAwaitableResultToWeakList() requires false
	{
	}

private:
	friend struct FWeakCoroutine;
	std::coroutine_handle<FWeakCoroutine::promise_type> Handle;
};


void FWeakCoroutine::Init(
	auto& Lifetime,
	TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
	TUniquePtr<FWeakCoroutineContext> Context)
{
	Context->Handle = Handle;
	Handle.promise().AddToWeakList(Lifetime);
	Handle.promise().LambdaCaptures = LambdaCaptures;
	Handle.resume();
}


class FWeakCoroutineHandle
{
public:
	void Init(std::coroutine_handle<FWeakCoroutine::promise_type> InHandle)
	{
		Handle = InHandle;
		bCoroutineDestroyed = Handle.promise().bDestroyed;
	}

	void Resume()
	{
		if (!Handle || *bCoroutineDestroyed)
		{
			return;
		}

		if (!Handle.promise().IsValid())
		{
			*bCoroutineDestroyed = true;
			Handle.destroy();
			return;
		}

		// TODO bCustomSuspended인 경우 Value를 복사하지 않고
		// 레퍼런스로 전달할 방법이 없을지 고민해보자
		if (Handle.promise().bCustomSuspended)
		{
			Handle.resume();
		}
	}

	void Destroy()
	{
		if (Handle && !*bCoroutineDestroyed)
		{
			*bCoroutineDestroyed = true;
			Handle.destroy();
		}
	}

private:
	TSharedPtr<bool> bCoroutineDestroyed;
	std::coroutine_handle<FWeakCoroutine::promise_type> Handle;
};


template <typename ValueType>
class TWeakAwaitableHandleImpl
{
public:
	TWeakAwaitableHandleImpl() = default;
	TWeakAwaitableHandleImpl(const TWeakAwaitableHandleImpl&&) = delete;
	TWeakAwaitableHandleImpl& operator=(const TWeakAwaitableHandleImpl&&) = delete;

	void SetHandle(std::coroutine_handle<FWeakCoroutine::promise_type> InHandle)
	{
		Handle.Init(InHandle);
	}

	template <typename ArgType>
	void SetValue(ArgType&& Arg)
	{
		Value.Emplace(Forward<ArgType>(Arg));
		Handle.Resume();
	}

	void Destroy()
	{
		Handle.Destroy();
	}

	const ValueType& GetValue() const { return *Value; }

	explicit operator bool() const { return !!Value; }

private:
	TOptional<ValueType> Value;
	FWeakCoroutineHandle Handle;
};


template <typename ValueType>
class TWeakAwaitableHandle
{
public:
	TWeakAwaitableHandle(TSharedPtr<TWeakAwaitableHandleImpl<ValueType>> InImpl)
		: Impl(InImpl)
	{
	}

	TWeakAwaitableHandle(const TWeakAwaitableHandle&) = delete;
	TWeakAwaitableHandle(TWeakAwaitableHandle&&) = default;
	TWeakAwaitableHandle& operator=(const TWeakAwaitableHandle&) = delete;
	TWeakAwaitableHandle& operator=(TWeakAwaitableHandle&&) = default;

	~TWeakAwaitableHandle()
	{
		if (Impl && !bSetValue)
		{
			Impl->Destroy();
		}
	}

	bool IsWaitingForValue() const
	{
		return Impl && !bSetValue;
	}

	template <typename ArgType>
	void SetValue(ArgType&& Arg)
	{
		check(!bSetValue);
		bSetValue = true;
		Impl->SetValue(Forward<ArgType>(Arg));
		Impl = nullptr;
	}

	template <typename ArgType>
	void SetValueIfNotSet(ArgType&& Arg)
	{
		if (!bSetValue)
		{
			SetValue(Forward<ArgType>(Arg));
		}
	}

	void Cancel()
	{
		if (!bSetValue)
		{
			bSetValue = true;
			Impl->Destroy();
		}
	}

	const ValueType& GetValue() const { return Impl->GetValue(); }

private:
	bool bSetValue = false;
	TSharedPtr<TWeakAwaitableHandleImpl<ValueType>> Impl;
};


template <typename ValueType>
class TWeakAwaitable
{
	static constexpr bool bObjectPtr = std::is_base_of_v<UObject, std::remove_pointer_t<std::decay_t<ValueType>>>;

public:
	TWeakAwaitable() = default;

	template <typename T> requires std::is_convertible_v<T, ValueType>
	TWeakAwaitable(T&& Ready)
	{
		Value->SetValue(Forward<T>(Ready));
	}

	TWeakAwaitableHandle<ValueType> GetHandle() const
	{
		check(!bHandleCreated);
		bHandleCreated = true;
		return {Value};
	}

	template <typename DelegateType>
	DelegateType CreateSetValueDelegate(UObject* Lifetime)
	{
		return CreateSetValueDelegate<DelegateType>(
			Lifetime,
			[]<typename ArgType>(ArgType&& Pass) -> decltype(auto)
			{
				return Forward<ArgType>(Pass);
			});
	}

	template <typename DelegateType, typename TransformFuncType>
	DelegateType CreateSetValueDelegate(UObject* Lifetime, TransformFuncType&& TransformFunc)
	{
		auto DelegateUnbinder = MakeShared<bool>();

		// TODO 현재 멀티캐스트 델리게이트가 복사될 경우 한 델리게이트가 SetValue를 완료해도
		// 다른 델리게이트들에서 핸들을 들고 있어서 해당 델리게이트들이 파괴되거나 한 번 호출되기 전까지
		// Value가 메모리에 유지됨 -> Value의 크기가 클 경우 메모리 낭비가 발생할 수 있으므로
		// indirection을 한 번 더 해서 Value가 아니라 포인터 크기 정도의 메모리만 들고 있게 해야함
		return DelegateType::CreateSPLambda(
			DelegateUnbinder,
			[
				SharedHandle = MakeShared<TWeakAwaitableHandle<ValueType>>(GetHandle()),
				DelegateUnbinder = DelegateUnbinder.ToSharedPtr(),
				Lifetime = TWeakObjectPtr{Lifetime},
				TransformFunc = Forward<TransformFuncType>(TransformFunc)
			]<typename... ArgTypes>(ArgTypes&&... Args) mutable
			{
				if (Lifetime.IsValid())
				{
					SharedHandle->SetValueIfNotSet(TransformFunc(Forward<ArgTypes>(Args)...));
				}

				DelegateUnbinder = nullptr;
			}
		);
	}

	template <typename MulticastDelegateType>
	void SetValueFromMulticastDelegate(UObject* Lifetime, MulticastDelegateType& MulticastDelegate)
	{
		MulticastDelegate.Add(CreateSetValueDelegate<MulticastDelegateType::FDelegate>(Lifetime));
	}

	template <typename MulticastDelegateType, typename TransformFuncType>
	void SetValueFromMulticastDelegate(UObject* Lifetime, MulticastDelegateType& MulticastDelegate, TransformFuncType&& TransformFunc)
	{
		MulticastDelegate.Add(CreateSetValueDelegate<MulticastDelegateType::FDelegate>(Lifetime, Forward<TransformFuncType>(TransformFunc)));
	}

	void SetNeverReady()
	{
		bNeverReady = true;

		if (Handle)
		{
			Value->Destroy();
		}
	}

	bool await_ready() const
	{
		return !bNeverReady && !!*Value;
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutine::promise_type> InHandle)
	{
		Handle = InHandle;
		Handle.promise().bCustomSuspended = true;
		Value->SetHandle(Handle);

		if (bNeverReady)
		{
			Value->Destroy();
		}
	}

	ValueType await_resume()
	{
		if (Handle)
		{
			Handle.promise().bCustomSuspended = false;

			if constexpr (bObjectPtr)
			{
				Handle.promise().AddToWeakList(Value->GetValue());
			}
		}

		return Value->GetValue();
	}

private:
	mutable bool bHandleCreated = false;
	std::coroutine_handle<FWeakCoroutine::promise_type> Handle;
	TSharedPtr<TWeakAwaitableHandleImpl<ValueType>> Value = MakeShared<TWeakAwaitableHandleImpl<ValueType>>();
	bool bNeverReady = false;
};


template <typename ValueType>
class TReadyWeakAwaitable
{
public:
	template <typename T>
	TReadyWeakAwaitable(T&& InValue)
		: Value(Forward<T>(InValue))
	{
	}

	bool await_ready() const
	{
		return true;
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutine::promise_type> InHandle)
	{
	}

	ValueType& await_resume() &
	{
		return Value;
	}

	ValueType&& await_resume() &&
	{
		return MoveTemp(Value);
	}

private:
	ValueType Value;
};


template <typename T>
class TValueGeneratorValueReceiver
{
public:
	template <typename U>
	void AddValue(U&& Value)
	{
		if (ValueAwaitableHandles.Num() > 0)
		{
			auto Handle = MoveTemp(ValueAwaitableHandles[0]);
			ValueAwaitableHandles.RemoveAt(0);
			Handle.SetValue(Forward<U>(Value));
		}
		else
		{
			Values.Add(Forward<U>(Value));
		}
	}

	TWeakAwaitable<T> GetValue()
	{
		if (Values.Num() > 0)
		{
			TWeakAwaitable<T> Ret{MoveTemp(Values[0])};
			Values.RemoveAt(0);
			return Ret;
		}

		TWeakAwaitable<T> Ret;
		ValueAwaitableHandles.Add(Ret.GetHandle());
		return Ret;
	}

private:
	TArray<T> Values;
	TArray<TWeakAwaitableHandle<T>> ValueAwaitableHandles;
};


template <typename T>
class TValueGenerator
{
public:
	TWeakPtr<TValueGeneratorValueReceiver<T>> GetReceiver() const
	{
		return Receiver;
	}

	TWeakAwaitable<T> Next()
	{
		return Receiver->GetValue();
	}

private:
	TSharedPtr<TValueGeneratorValueReceiver<T>> Receiver = MakeShared<TValueGeneratorValueReceiver<T>>();
};


template <typename T>
TReadyWeakAwaitable<T> CreateReadyWeakAwaitable(T&& Value)
{
	return TReadyWeakAwaitable<T>{Forward<T>(Value)};
}


template <
	typename MulticastDelegateType,
	typename ReturnType = std::decay_t<typename TGetFirstParam<typename MulticastDelegateType::FDelegate::TFuncType>::Type>>
TWeakAwaitable<ReturnType> WaitForBroadcast(UObject* Lifetime, MulticastDelegateType& Delegate)
{
	TWeakAwaitable<ReturnType> Ret;
	Ret.SetValueFromMulticastDelegate(Lifetime, Delegate);
	return Ret;
}


template <
	typename MulticastDelegateType,
	typename TransformFuncType>
auto WaitForBroadcast(UObject* Lifetime, MulticastDelegateType& Delegate, TransformFuncType&& TransformFunc)
{
	using DelegateReturnType = typename TGetFirstParam<typename MulticastDelegateType::FDelegate::TFuncType>::Type;
	using ReturnType = std::decay_t<decltype(std::declval<TransformFuncType>()(std::declval<DelegateReturnType>()))>;

	TWeakAwaitable<ReturnType> Ret;
	Ret.SetValueFromMulticastDelegate(Lifetime, Delegate, Forward<TransformFuncType>(TransformFunc));
	return Ret;
}


template <typename FuncType>
void RunWeakCoroutine(const auto& Lifetime, FuncType&& Func)
{
	auto LambdaCaptures = MakeShared<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<FWeakCoroutineContext>();
	(*LambdaCaptures)(*Context).Init(Lifetime, LambdaCaptures, MoveTemp(Context));
}


template <typename T>
TWeakAwaitable<T> CreateAwaitableToArray(bool bCondition, const T& ReadyValue, TArray<TWeakAwaitableHandle<T>>& Array)
{
	if (bCondition)
	{
		return ReadyValue;
	}

	TWeakAwaitable<T> Ret;
	Array.Add(Ret.GetHandle());
	return Ret;
}


template <typename T, typename U>
void FlushAwaitablesArray(TArray<TWeakAwaitableHandle<T>>& Array, const U& Value)
{
	for (auto& Each : Array)
	{
		Each.SetValue(Value);
	}
	Array.Empty();
}


template <typename T, typename DelegateType>
TValueGenerator<T> CreateMulticastValueGenerator(const TArray<T>& ReadyValues, DelegateType& MulticastDelegate)
{
	TValueGenerator<T> Ret;

	auto Receiver = Ret.GetReceiver();
	for (const T& Each : ReadyValues)
	{
		Receiver.Pin()->AddValue(Each);
	}

	MulticastDelegate.Add(DelegateType::FDelegate::CreateSPLambda(Receiver.Pin().ToSharedRef(), [Receiver]<typename U>(U&& NewValue)
	{
		Receiver.Pin()->AddValue(Forward<U>(NewValue));
	}));

	return Ret;
}


using FWeakAwaitableInt32 = TWeakAwaitable<int32>;
using FWeakAwaitableHandleInt32 = TWeakAwaitableHandle<int32>;
