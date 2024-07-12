// Copyright Epic Games, Inc. All Rights Reserved.

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
		TUniquePtr<FWeakCoroutineContext> Context;
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
		TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
		TUniquePtr<FWeakCoroutineContext> InContext);

	void Init(
		auto& Lifetime,
		TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
		TUniquePtr<FWeakCoroutineContext> InContext);

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
	decltype(auto) AddToWeakList(T&& Weak)
	{
		Handle.promise().AddToWeakList(Weak);
		return Forward<T>(Weak);
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


inline void FWeakCoroutine::Init(
	TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
	TUniquePtr<FWeakCoroutineContext> InContext)
{
	Handle.promise().Context = MoveTemp(InContext);
	Handle.promise().Context->Handle = Handle;
	Handle.promise().LambdaCaptures = LambdaCaptures;
	Handle.resume();
}


void FWeakCoroutine::Init(
	auto& Lifetime,
	TSharedPtr<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>> LambdaCaptures,
	TUniquePtr<FWeakCoroutineContext> InContext)
{
	Handle.promise().Context = MoveTemp(InContext);
	Handle.promise().Context->Handle = Handle;
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
		check(!Value.IsSet());
		Value.Emplace(Forward<ArgType>(Arg));
		Handle.Resume();
	}

	template <typename ArgType>
	void SetValueIfNotSet(ArgType&& Arg)
	{
		if (!Value.IsSet())
		{
			SetValue(Forward<ArgType>(Arg));
		}
	}

	void Destroy()
	{
		Handle.Destroy();
	}

	bool IsValueSet() const { return !!Value; }

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
	TWeakAwaitableHandle& operator=(const TWeakAwaitableHandle&) = delete;

	TWeakAwaitableHandle(TWeakAwaitableHandle&&) = default;
	TWeakAwaitableHandle& operator=(TWeakAwaitableHandle&&) = default;

	~TWeakAwaitableHandle()
	{
		if (Impl && !bSetValue)
		{
			Impl->Destroy();
		}
	}

	template <typename ArgType>
	void SetValue(ArgType&& Arg)
	{
		check(!bSetValue);
		bSetValue = true;
		Impl->SetValue(Forward<ArgType>(Arg));
		Impl = nullptr;
	}

	void Cancel()
	{
		bSetValue = true;
		Impl->Destroy();
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
	DelegateType CreateSetValueDelegate()
	{
		return CreateSetValueDelegate<DelegateType>(
			[]<typename ArgType>(ArgType&& Pass) -> decltype(auto)
			{
				return Forward<ArgType>(Pass);
			});
	}

	template <typename DelegateType, typename TransformFuncType>
	DelegateType CreateSetValueDelegate(TransformFuncType&& TransformFunc)
	{
		// 핸들을 만들어서 다른 곳에서 핸들을 만들지 못하게 한다.
		GetHandle();

		struct FAwaitableCanceler
		{
			TWeakPtr<TWeakAwaitableHandleImpl<ValueType>> WeakAwaitable;

			~FAwaitableCanceler()
			{
				if (auto Alive = WeakAwaitable.Pin(); Alive && !Alive->IsValueSet())
				{
					Alive->Destroy();
				}
			}
		};

		// TODO 아래 코드에 대한 설명
		auto FireDelegateOnce = MakeShared<bool>(true);
		return DelegateType::CreateSPLambda(
			FireDelegateOnce,
			[
				FireDelegateOnce = FireDelegateOnce.ToSharedPtr(),
				Canceler = MakeShared<FAwaitableCanceler>(Value),
				TransformFunc = Forward<TransformFuncType>(TransformFunc)
			]<typename... ArgTypes>(ArgTypes&&... Args) mutable
			{
				if (auto Alive = Canceler->WeakAwaitable.Pin())
				{
					Alive->SetValueIfNotSet(TransformFunc(Forward<ArgTypes>(Args)...));
				}
				FireDelegateOnce = nullptr;
			}
		);
	}

	template <typename MulticastDelegateType>
	void SetValueFromMulticastDelegate(MulticastDelegateType& MulticastDelegate)
	{
		MulticastDelegate.Add(CreateSetValueDelegate<typename MulticastDelegateType::FDelegate>());
	}

	template <typename MulticastDelegateType, typename TransformFuncType>
	void SetValueFromMulticastDelegate(MulticastDelegateType& MulticastDelegate, TransformFuncType&& TransformFunc)
	{
		MulticastDelegate.Add(CreateSetValueDelegate<typename MulticastDelegateType::FDelegate>(Forward<TransformFuncType>(TransformFunc)));
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
class TValueStreamValueReceiver
{
public:
	template <typename U>
	void AddValue(U&& Value)
	{
		check(!bEnded);

		if (ValueHandlers.Num() > 0)
		{
			auto Handle = MoveTemp(ValueHandlers[0]);
			ValueHandlers.RemoveAt(0);
			Handle(Forward<U>(Value));
		}
		else
		{
			Values.Add(Forward<U>(Value));
		}
	}

	T PopValue()
	{
		T Ret = MoveTemp(Values[0]);
		Values.RemoveAt(0);
		return Ret;
	}

	void End()
	{
		bEnded = true;
		for (auto& Each : ValueHandlers)
		{
			Each({});
		}
		ValueHandlers.Empty();
	}

	TWeakAwaitable<T> WaitForValue()
	{
		if (Values.Num() > 0)
		{
			TWeakAwaitable<T> Ret{MoveTemp(Values[0])};
			Values.RemoveAt(0);
			return Ret;
		}

		if (!bEnded)
		{
			TWeakAwaitable<T> Ret;
			ValueHandlers.Add([Handle = Ret.GetHandle()](TOptional<T> NewValue) mutable
			{
				if (NewValue)
				{
					Handle.SetValue(MoveTemp(*NewValue));
				}
			});
			return Ret;
		}

		TWeakAwaitable<T> Ret;
		Ret.SetNeverReady();
		return Ret;
	}

	TWeakAwaitable<bool> WaitForValueIfNotEnd()
	{
		if (bEnded)
		{
			return false;
		}

		if (Values.Num() > 0)
		{
			return true;
		}

		TWeakAwaitable<bool> Ret;
		ValueHandlers.Add([this, Handle = Ret.GetHandle()](TOptional<T> NewValue) mutable
		{
			if (NewValue)
			{
				Values.Add(MoveTemp(*NewValue));
			}

			Handle.SetValue(NewValue.IsSet());
		});

		return Ret;
	}

private:
	bool bEnded = false;
	TArray<T> Values;
	TArray<TUniqueFunction<void(TOptional<T>)>> ValueHandlers;
};


template <typename T>
class TValueStream
{
public:
	TValueStream() = default;

	// 복사 허용하면 내가 Next하고 다른 애가 Next하면 다른 애의 Next는 나 다음에 호출되므로
	// 사실상 Next가 아니라 NextNext임 이게 좀 혼란스러울 수 있으므로 일단 막음
	TValueStream(const TValueStream&) = delete;
	TValueStream& operator=(const TValueStream&) = delete;

	TValueStream(TValueStream&&) = default;
	TValueStream& operator=(TValueStream&&) = default;

	TWeakPtr<TValueStreamValueReceiver<T>> GetReceiver() const
	{
		return Receiver;
	}

	TWeakAwaitable<T> Next()
	{
		return Receiver->WaitForValue();
	}

	TWeakAwaitable<bool> NextIfNotEnd()
	{
		return Receiver->WaitForValueIfNotEnd();
	}

	T Pop()
	{
		return Receiver->PopValue();
	}

private:
	TSharedPtr<TValueStreamValueReceiver<T>> Receiver = MakeShared<TValueStreamValueReceiver<T>>();
};


template <typename T>
TReadyWeakAwaitable<T> CreateReadyWeakAwaitable(T&& Value)
{
	return TReadyWeakAwaitable<T>{Forward<T>(Value)};
}


template <
	typename MulticastDelegateType,
	typename ReturnType = std::decay_t<typename TGetFirstParam<typename MulticastDelegateType::FDelegate::TFuncType>::Type>>
TWeakAwaitable<ReturnType> WaitForBroadcast(MulticastDelegateType& Delegate)
{
	TWeakAwaitable<ReturnType> Ret;
	Ret.SetValueFromMulticastDelegate(Delegate);
	return Ret;
}


template <
	typename MulticastDelegateType,
	typename TransformFuncType>
auto WaitForBroadcast(MulticastDelegateType& Delegate, TransformFuncType&& TransformFunc)
{
	using DelegateReturnType = typename TGetFirstParam<typename MulticastDelegateType::FDelegate::TFuncType>::Type;
	using ReturnType = std::decay_t<decltype(std::declval<TransformFuncType>()(std::declval<DelegateReturnType>()))>;

	TWeakAwaitable<ReturnType> Ret;
	Ret.SetValueFromMulticastDelegate(Delegate, Forward<TransformFuncType>(TransformFunc));
	return Ret;
}


template <typename FuncType>
void RunCoroutine(FuncType&& Func)
{
	auto LambdaCaptures = MakeShared<TUniqueFunction<FWeakCoroutine(FWeakCoroutineContext&)>>(Forward<FuncType>(Func));
	auto Context = MakeUnique<FWeakCoroutineContext>();
	(*LambdaCaptures)(*Context).Init(LambdaCaptures, MoveTemp(Context));
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
TValueStream<T> CreateMulticastValueStream(const TArray<T>& ReadyValues, DelegateType& MulticastDelegate)
{
	TValueStream<T> Ret;

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


template <typename T, typename PredicateType>
TWeakAwaitable<T> FirstInStream(TValueStream<T>&& Stream, PredicateType&& Predicate)
{
	TWeakAwaitable<T> Ret;
	RunCoroutine([
			Handle = Ret.GetHandle(),
			Stream = MoveTemp(Stream),
			Predicate = Forward<PredicateType>(Predicate)](auto&) mutable -> FWeakCoroutine
		{
			while (true)
			{
				auto Value = co_await Stream.Next();
				if (Predicate(Value))
				{
					Handle.SetValue(MoveTemp(Value));
					break;
				}
			}
		});
	return Ret;
}


using FWeakAwaitableInt32 = TWeakAwaitable<int32>;
using FWeakAwaitableHandleInt32 = TWeakAwaitableHandle<int32>;
