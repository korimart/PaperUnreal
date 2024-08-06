// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FilterAwaitable.h"
#include "MinimalAbortableCoroutine.h"
#include "PaperUnreal/GameFramework2/Utils.h"


template <typename InnerAwaitableType, typename CoroutineGetterType>
class TWhileTrueAwaitable
{
public:
	template <typename AwaitableType, typename CoGetter>
	TWhileTrueAwaitable(AwaitableType&& Awaitable, CoGetter&& Getter)
		: Inner(Forward<AwaitableType>(Awaitable)), CoroutineGetter(Forward<CoGetter>(Getter))
	{
	}

	bool await_ready() const
	{
		// 이 Awaitable은 한 번만 사용될 것을 염두에 두고 작성되었음
		check(!Worker.IsSet());
		return false;
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		Worker = Start(Forward<HandleType>(Handle));
	}

	// TODO inner가 error reporting이 아니면 non failable 반환하도록 변경
	TFailableResult<std::monostate> await_resume()
	{
		return Errors;
	}
	
	void await_abort()
	{
		Worker->Abort();
	}

private:
	InnerAwaitableType Inner;
	CoroutineGetterType CoroutineGetter;
	TOptional<FMinimalAbortableCoroutine> Worker;
	TArray<UFailableResultError*> Errors;

	FMinimalAbortableCoroutine Start(auto Handle)
	{
		while (true)
		{
			// TODO 이거랑 같은 패턴이 Filter Awaitable에도 반복되어 있음
			// Inner Coroutine 만들어서 Error forwarding 기능 구현 필요
			auto Failable0 = co_await (Inner | Awaitables::If(true) | Awaitables::NoDestroy());
			if (ForwardErrorsIfError(Handle, Failable0))
			{
				break;
			}
			
			auto Coroutine = CoroutineGetter();
			auto F = Finally([&](){ Coroutine.Abort(); });
			
			auto Failable1 = co_await (Inner | Awaitables::If(false) | Awaitables::NoDestroy());
			if (ForwardErrorsIfError(Handle, Failable1))
			{
				break;
			}
		}
	}

	bool ForwardErrorsIfError(const auto& Handle, const auto& Failable)
	{
		if (Failable)
		{
			return false;
		}

		if (Failable.template ContainsAnyOf<UNoDestroyError>())
		{
			Handle.destroy();
			return true;
		}

		Errors = Failable.GetErrors();
		Handle.resume();
		return true;
	}
};

template <typename AwaitableType, typename CoroutineGetterType>
TWhileTrueAwaitable(AwaitableType&&, CoroutineGetterType&&) -> TWhileTrueAwaitable<AwaitableType, std::decay_t<CoroutineGetterType>>;


template <typename CoroutineGetterType>
struct TWhileTrueAdaptor : TAwaitableAdaptorBase<TWhileTrueAdaptor<CoroutineGetterType>>
{
	std::decay_t<CoroutineGetterType> CoroutineGetter;

	TWhileTrueAdaptor(const std::decay_t<CoroutineGetterType>& Getter)
		: CoroutineGetter(Getter)
	{
	}
	
	TWhileTrueAdaptor(std::decay_t<CoroutineGetterType>&& Getter)
		: CoroutineGetter(MoveTemp(Getter))
	{
	}
	
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, TWhileTrueAdaptor Adaptor)
	{
		return TWhileTrueAwaitable{Forward<AwaitableType>(Awaitable), MoveTemp(Adaptor.CoroutineGetter)};
	}
};

namespace Awaitables
{
	template <typename CoroutineGetterType>
	auto WhileTrue(CoroutineGetterType&& Predicate)
	{
		return TWhileTrueAdaptor<CoroutineGetterType>{Forward<CoroutineGetterType>(Predicate)};
	}
}