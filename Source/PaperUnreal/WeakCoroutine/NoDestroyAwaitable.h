// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ErrorReporting.h"
#include "NoDestroyAwaitable.generated.h"


UCLASS()
class UNoDestroyError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UNoDestroyError* DestroyCalled()
	{
		return NewError<UNoDestroyError>(TEXT("Inner Awaitable이 destroy를 요청했으므로 Error로 변경해서 resume합니다"));
	}
};


template <typename InnerAwaitableType>
class TNoDestroyAwaitable
{
public:
	using ResultType = typename std::conditional_t
	<
		CErrorReportingAwaitable<InnerAwaitableType>,
		TErrorReportResultType<InnerAwaitableType>,
		TIdentity<decltype(std::declval<InnerAwaitableType>().await_resume())>
	>::Type;

	template <typename AwaitableType>
	TNoDestroyAwaitable(AwaitableType&& Awaitable)
		: Inner(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TNoDestroyAwaitable>);
	}

	bool await_ready() const
	{
		return Inner.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle)
	{
		struct FDestroyChecker
		{
			TNoDestroyAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const { Handle.resume(); }

			void destroy() const
			{
				This->bDestroyed = true;
				Handle.resume();
			}

			auto& promise() const { return Handle.promise(); }
		};

		bDestroyed = false;
		return Inner.await_suspend(FDestroyChecker{this, Forward<HandleType>(Handle)});
	}

	TFailableResult<ResultType> await_resume()
	{
		if (bDestroyed)
		{
			return UNoDestroyError::DestroyCalled();
		}

		return Inner.await_resume();
	}
	
	void await_abort()
	{
		Inner.await_abort();
	}

private:
	bool bDestroyed = false;
	InnerAwaitableType Inner;
};

template <typename AwaitableType>
TNoDestroyAwaitable(AwaitableType&&) -> TNoDestroyAwaitable<AwaitableType>;


struct FNoDestroyAdaptor : TAwaitableAdaptorBase<FNoDestroyAdaptor>
{
	template <CAwaitable AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, FNoDestroyAdaptor)
	{
		return TNoDestroyAwaitable{Forward<AwaitableType>(Awaitable)};
	}
};


namespace Awaitables
{
	inline FNoDestroyAdaptor NoDestroy()
	{
		return {};
	}
}