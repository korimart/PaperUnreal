// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>


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


template <typename>
class TWeakAwaitable;

template <typename>
class TReadyWeakAwaitable;


struct FWeakCoroutine
{
	struct promise_type
	{
		TWeakObjectPtr<UObject> Lifetime;
		TSharedPtr<TUniqueFunction<FWeakCoroutine()>> LambdaCaptures;
		bool bCustomSuspended = false;
		TSharedPtr<bool> bDestroyed = MakeShared<bool>(false);

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
	};

	void Init(UObject* Lifetime, TSharedPtr<TUniqueFunction<FWeakCoroutine()>> LambdaCaptures)
	{
		Handle.promise().Lifetime = Lifetime;
		Handle.promise().LambdaCaptures = LambdaCaptures;
		Handle.resume();
	}

private:
	std::coroutine_handle<promise_type> Handle;

	FWeakCoroutine(std::coroutine_handle<promise_type> InHandle)
		: Handle(InHandle)
	{
	}
};


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

		if (!Handle.promise().Lifetime.IsValid())
		{
			*bCoroutineDestroyed = true;
			Handle.destroy();
			return;
		}

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
		Value = Forward<ArgType>(Arg);
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

	template <typename ArgType>
	void SetValue(ArgType&& Arg)
	{
		check(!bSetValue);
		bSetValue = true;
		Impl->SetValue(Forward<ArgType>(Arg));
	}

	template <typename ArgType>
	void SetValueIfNotSet(ArgType&& Arg)
	{
		if (!bSetValue)
		{
			SetValue(Forward<ArgType>(Arg));
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
public:
	TWeakAwaitable() = default;

	template <typename T>
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

	// TODO write a test for this
	template <typename DelegateType>
	void SetValueFromMulticastDelegate(UObject* Lifetime, DelegateType& MulticastDelegate)
	{
		auto DelegateUnbinder = MakeShared<bool>();

		// TODO 현재 멀티캐스트 델리게이트가 복사될 경우 한 델리게이트가 SetValue를 완료해도
		// 다른 델리게이트들에서 핸들을 들고 있어서 해당 델리게이트들이 파괴되거나 한 번 호출되기 전까지
		// Value가 메모리에 유지됨 -> Value의 크기가 클 경우 메모리 낭비가 발생할 수 있으므로
		// indirection을 한 번 더 해서 Value가 아니라 포인터 크기 정도의 메모리만 들고 있게 해야함
		auto Delegate = std::decay_t<decltype(MulticastDelegate)>::FDelegate::CreateSPLambda(
			DelegateUnbinder,
			[
				SharedHandle = MakeShared<TWeakAwaitableHandle<ValueType>>(GetHandle()),
				DelegateUnbinder = DelegateUnbinder.ToSharedPtr(),
				Lifetime = TWeakObjectPtr{Lifetime}
			]<typename ArgType>(ArgType&& Arg) mutable
			{
				if (Lifetime.IsValid())
				{
					SharedHandle->SetValueIfNotSet(Forward<ArgType>(Arg));
				}

				DelegateUnbinder = nullptr;
			}
		);

		MulticastDelegate.Add(MoveTemp(Delegate));
	}

	bool await_ready() const
	{
		return !!*Value;
	}

	void await_suspend(std::coroutine_handle<FWeakCoroutine::promise_type> InHandle)
	{
		Handle = InHandle;
		Handle.promise().bCustomSuspended = true;
		Value->SetHandle(Handle);
	}

	ValueType await_resume()
	{
		if (Handle)
		{
			Handle.promise().bCustomSuspended = false;
		}
		
		return Value->GetValue();
	}

private:
	mutable bool bHandleCreated = false;
	std::coroutine_handle<FWeakCoroutine::promise_type> Handle;
	TSharedPtr<TWeakAwaitableHandleImpl<ValueType>> Value = MakeShared<TWeakAwaitableHandleImpl<ValueType>>();
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
TReadyWeakAwaitable<T> CreateReadyWeakAwaitable(T&& Value)
{
	return TReadyWeakAwaitable<T>{Forward<T>(Value)};
}


template <typename FuncType>
void RunWeakCoroutine(UObject* Lifetime, FuncType&& Func)
{
	auto LambdaCaptures = MakeShared<TUniqueFunction<FWeakCoroutine()>>(Forward<FuncType>(Func));
	(*LambdaCaptures)().Init(Lifetime, LambdaCaptures);
}


using FWeakAwaitableInt32 = TWeakAwaitable<int32>;
using FWeakAwaitableHandleInt32 = TWeakAwaitableHandle<int32>;
