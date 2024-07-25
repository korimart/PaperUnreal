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


UCLASS()
class UErrorReportingError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UErrorReportingError* ObjectExpired()
	{
		return NewError<UErrorReportingError>(TEXT(
			"TFailable가 UObject를 보관하기 시작할 때는 Valid였는데 꺼낼 때는 Invalid가 됨"
			"(TFailable이 TStrongObjectPtr를 사용하여 레퍼런스를 유지하기 때문에 "
			"명시적으로 Garbage가 된 경우에만 발생 가능)"));
	}

	static UErrorReportingError* InnerAwaitableAborted()
	{
		return NewError<UErrorReportingError>(TEXT(
			"TAlwaysResumingAwaitable: inner awaitable이 코루틴을 파괴했으므로 에러로 바꿔서 resume 합니다"));
	}
};


template <typename ResultType>
struct TFailableResultStorage
{
	template <typename T>
	TFailableResultStorage(T&& Value)
		: Result(Forward<T>(Value))
	{
	}

	bool Expired() const { return false; }
	ResultType& Get() { return Result; }
	const ResultType& Get() const { return Result; }

private:
	ResultType Result;
};


template <typename ResultType> requires std::is_base_of_v<UObject, std::decay_t<ResultType>>
struct TFailableResultStorage<ResultType*>
{
	TFailableResultStorage(ResultType* ValidResult)
		: Result(ValidResult)
	{
		check(::IsValid(ValidResult));
	}

	bool Expired() const { return !::IsValid(Result.Get()); }
	ResultType* Get() const { return Result.Get(); }

private:
	TStrongObjectPtr<ResultType> Result;
};


template <typename InResultType>
class TFailableResult
{
public:
	using ResultType = InResultType;

	TFailableResult(UFailableResultError* Error)
	{
		AddError(Error);
	}

	template <typename T>
	TFailableResult(T&& Value) requires !std::is_convertible_v<T, UFailableResultError*>
		: Result(Forward<T>(Value))
	{
	}

	TFailableResult(TFailableResult&&) = default;
	TFailableResult& operator=(TFailableResult&&) = default;

	void AddError(UFailableResultError* Error)
	{
		Errors.Emplace(Error);
	}

	bool Failed() const
	{
		return (Result.IsSet() && Result->Expired()) || Errors.Num() > 0;
	}

	bool Succeeded() const
	{
		return !Failed();
	}

	decltype(auto) GetResult()
	{
		check(Succeeded());
		return Result->Get();
	}
	
	decltype(auto) GetResult() const
	{
		check(Succeeded());
		return Result->Get();
	}

	TArray<UFailableResultError*> GetErrors() const
	{
		TArray<UFailableResultError*> Ret;
		if (Result.IsSet() && Result->Expired())
		{
			Ret.Add(UErrorReportingError::ObjectExpired());
		}
		Algo::Transform(Errors, Ret, [](auto& Each) { return Each.Get(); });
		return Ret;
	}

private:
	TOptional<TFailableResultStorage<ResultType>> Result;
	TArray<TStrongObjectPtr<UFailableResultError>> Errors;
};


template <typename AwaitableType>
concept CErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	typename AwaitableType::ResultType;
	{ Awaitable.await_resume() } -> CInstantiationOf<TFailableResult>;
};


template <typename AwaitableType>
concept CNonErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	{ Awaitable.await_resume() } -> CNotInstantiationOf<TFailableResult>;
};


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
	bool await_suspend(HandleType&& Handle)
	{
		if (Awaitable.await_ready())
		{
			Result = Awaitable.await_resume();
			if (Result->Failed())
			{
				Handle.destroy();
				return true;
			}
			return false;
		}

		struct FFailChecker
		{
			TErrorRemovedAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const
			{
				This->Result = This->Awaitable.await_resume();
				This->Result->Failed() ? Handle.destroy() : Handle.resume();
			}

			void destroy() const { Handle.destroy(); }
		};

		Awaitable.await_suspend(FFailChecker{this, Forward<HandleType>(Handle)});
		return true;
	}

	typename AwaitableType::ResultType await_resume()
	{
		// 이곳의 std::move 사용은 의도적임
		// MoveTemp는 prvalue가 arguemnt로 들어왔는지 static_assert로 체크하는데
		// GetResult의 결과는 ResultType이 포인터 타입일 때 prvalue이고 그 외에는 lvalue reference임
		// lvalue reference일 때는 move하고 포인터 타입일 때는 그냥 반환하는 것이 목적이므로 std::move를 사용함
		return std::move(Result->GetResult());
	}

private:
	AwaitableType Awaitable;
	TOptional<TFailableResult<typename AwaitableType::ResultType>> Result;
};


template <CNonErrorReportingAwaitable AwaitableType>
class TAlwaysResumingAwaitable
{
public:
	using ResultType = decltype(std::declval<AwaitableType>().await_resume());
	
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

	TFailableResult<ResultType> await_resume()
	{
		if (bAborted)
		{
			return UErrorReportingError::InnerAwaitableAborted();
		}

		return Awaitable.await_resume();
	}

private:
	bool bAborted = false;
	AwaitableType Awaitable;
};
