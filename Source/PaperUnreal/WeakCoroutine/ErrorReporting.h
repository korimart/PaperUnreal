// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <source_location>

#include "CoreMinimal.h"
#include "TypeTraits.h"
#include "ErrorReporting.generated.h"


UCLASS()
class UFailableResultError : public UObject
{
	GENERATED_BODY()

public:
	FString What;
	std::source_location SourceLocation;
};


template <typename ErrorType = UFailableResultError>
ErrorType* NewError(const FString& What = TEXT("No error message was given"), std::source_location SourceLocation = {})
{
	ErrorType* Ret = NewObject<ErrorType>();
	Ret->What = What;
	Ret->SourceLocation = SourceLocation;
	return Ret;
}


template <typename InResultType>
class TFailableResult
{
public:
	using ResultType = InResultType;
	
	template <typename T>
	TFailableResult(T&& Value)
		requires std::is_constructible_v<ResultType, T> && !std::is_constructible_v<UFailableResultError*, T>
		: Result(Forward<T>(Value))
	{
	}

	TFailableResult(UFailableResultError* Error)
	{
		AddError(Error);
	}

	TFailableResult(TFailableResult&&) = default;
	TFailableResult& operator=(TFailableResult&&) = default;

	void AddError(UFailableResultError* Error)
	{
		check(!Result.IsSet());
		Errors.Emplace(Error);
	}

	bool Failed() const
	{
		return Errors.Num() > 0;
	}

	bool Succeeded() const
	{
		return Result.IsSet();
	}

	const ResultType& GetResult() const
	{
		check(Succeeded());
		return Result.GetValue();
	}

	TArray<UFailableResultError*> GetErrors() const
	{
		TArray<UFailableResultError*> Ret;
		Algo::Transform(Errors, Ret, [](auto& Each){ return Each.Get(); });
		return Ret;
	}

private:
	TOptional<ResultType> Result;
	TArray<TStrongObjectPtr<UFailableResultError>> Errors;
};


template <typename AwaitableType>
concept CErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	{ Awaitable.await_resume() } -> CInstantiationOf<TFailableResult>;
	{ Awaitable.Failed() } -> std::same_as<bool>;
};


template <typename AwaitableType>
concept CNonErrorReportingAwaitable = !CErrorReportingAwaitable<AwaitableType>;


template <CErrorReportingAwaitable AwaitableType>
class TErrorRemovedAwaitable
{
public:
	TErrorRemovedAwaitable(AwaitableType&& InAwaitable)
		: Awaitable(MoveTemp(InAwaitable))
	{
		static_assert(CNonErrorReportingAwaitable<TErrorRemovedAwaitable>);
	}

	TErrorRemovedAwaitable(const TErrorRemovedAwaitable&) = delete;
	TErrorRemovedAwaitable& operator=(const TErrorRemovedAwaitable&) = delete;

	TErrorRemovedAwaitable(TErrorRemovedAwaitable&&) = default;
	TErrorRemovedAwaitable& operator=(TErrorRemovedAwaitable&&) = default;

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		if (Awaitable.await_ready() && Awaitable.Failed())
		{
			Handle.destroy();
			return;
		}

		struct FFailChecker
		{
			TErrorRemovedAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const { This->Awaitable.Failed() ? Handle.destroy() : Handle.resume(); }
			void destroy() const { Handle.destroy(); }
		};

		Awaitable.await_suspend(FFailChecker{this, Forward<HandleType>(Handle)});
	}

	auto await_resume()
	{
		return Awaitable.await_resume().GetResult();
	}

private:
	AwaitableType Awaitable;
};


template <CNonErrorReportingAwaitable AwaitableType>
class TAlwaysResumingAwaitable
{
public:
	TAlwaysResumingAwaitable(AwaitableType&& InAwaitable)
		: Awaitable(MoveTemp(InAwaitable))
	{
		static_assert(CErrorReportingAwaitable<TAlwaysResumingAwaitable>);
	}

	TAlwaysResumingAwaitable(const TAlwaysResumingAwaitable&) = delete;
	TAlwaysResumingAwaitable& operator=(const TAlwaysResumingAwaitable&) = delete;

	TAlwaysResumingAwaitable(TAlwaysResumingAwaitable&&) = default;
	TAlwaysResumingAwaitable& operator=(TAlwaysResumingAwaitable&&) = default;

	bool await_ready() const
	{
		return Awaitable.await_ready();
	}

	template <typename HandleType>
	void await_suspend(HandleType&& Handle)
	{
		struct FAbortChecker
		{
			TAlwaysResumingAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() { Handle.resume(); }

			void destroy()
			{
				This->bAborted = true;
				Handle.resume();
			}
		};

		bAborted = false;
		Awaitable.await_suspend(FAbortChecker{this, Forward<HandleType>(Handle)});
	}

	TFailableResult<decltype(std::declval<AwaitableType>().await_resume())> await_resume()
	{
		if (bAborted)
		{
			return NewError(TEXT("TAlwaysResumingAwaitable: inner awaitable이 abort 했으므로 에러로 resume 합니다"));
		}
		
		return Awaitable.await_resume();
	}

	bool Failed() const
	{
		return bAborted;
	}

private:
	bool bAborted = false;
	AwaitableType Awaitable;
};