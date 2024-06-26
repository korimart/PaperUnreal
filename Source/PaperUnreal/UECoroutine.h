// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>


template <typename...>
struct TFalse
{
	static constexpr bool Value = false;
};


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
		
		template <typename ValueType>
		decltype(auto) await_transform(TReadyWeakAwaitable<ValueType>&& Awaitable)
		{
			return Forward<TReadyWeakAwaitable<ValueType>>(Awaitable);
		}

		template <typename ValueType>
		decltype(auto) await_transform(TWeakAwaitable<ValueType>&& Awaitable)
		{
			return Forward<TWeakAwaitable<ValueType>>(Awaitable);
		}

		template <typename AwaitableType>
		decltype(auto) await_transform(AwaitableType&& Awaitable)
		{
			// TODO support
			static_assert(TFalse<AwaitableType>::Value);
			return Forward<AwaitableType>(Awaitable);
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

	const ValueType& GetValue() const { return Impl->GetValue(); }

private:
	bool bSetValue = false;
	TSharedPtr<TWeakAwaitableHandleImpl<ValueType>> Impl;
};


template <typename ValueType>
class TWeakAwaitable
{
public:
	TWeakAwaitableHandle<ValueType> GetHandle() const
	{
		return {Value};
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
		Handle.promise().bCustomSuspended = false;
		return Value->GetValue();
	}

private:
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
