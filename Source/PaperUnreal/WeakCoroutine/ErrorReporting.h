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
};


template <typename ErrorType = UFailableResultError>
ErrorType* NewError(const FString& What = TEXT("No error message was given"))
{
	ErrorType* Ret = NewObject<ErrorType>();
	Ret->What = What;
	return Ret;
}


UCLASS()
class UErrorReportingError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UErrorReportingError* InvalidObject()
	{
		return NewError<UErrorReportingError>(TEXT("TFailable에 저장된 UObject가 Garbage임"
			"(애초에 쓰레기로 초기화 되었을 수도 있고 도중에 쓰레기가 되었을 수도 있음. "
			"하지만 TFailable이 Strong Object Pointer를 유지하기 때문에 레퍼런스 소실에 의한 것이 아니라 "
			"누군가가 명시적으로 쓰레기로 만든 것임)"));
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
	TFailableResultStorage(const ResultType& Value)
		: Result(Value)
	{
	}
	
	TFailableResultStorage(ResultType&& Value)
		: Result(MoveTemp(Value))
	{
	}

	template <typename... ArgTypes>
	TFailableResultStorage(EInPlace, ArgTypes&&... Args)
		: Result(Forward<ArgTypes>(Args)...)
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
		if (ValidResult == nullptr)
		{
			bInitializedWithNull = true;
		}
	}

	bool Expired() const { return !bInitializedWithNull && !::IsValid(Result.Get()); }
	ResultType* Get() const { return Result.Get(); }

private:
	bool bInitializedWithNull = false;
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

	TFailableResult(const TArray<UFailableResultError*>& Errors)
	{
		for (UFailableResultError* Each : Errors)
		{
			AddError(Each);
		}
	}

	/**
	 * ResultType이 포인터 타입일 때 TFailableResult를 nullptr로 초기화하는 경우
	 * 값이 nullptr인 것이므로 에러로 초기화되지 않도록 (위에 UFailableResultError* 받는 오버로드로 가지 않도록) 여기서 처리함
	 */
	TFailableResult(std::nullptr_t)
		: Result(nullptr)
	{
	}

	TFailableResult(const ResultType& Value)
		: Result(Value)
	{
	}
	
	TFailableResult(ResultType&& Value)
		: Result(MoveTemp(Value))
	{
	}
	
	template <typename... ArgTypes>
	TFailableResult(EInPlace, ArgTypes&&... Args)
		: Result(InPlace, InPlace, Forward<ArgTypes>(Args)...)
	{
	}

	TFailableResult(TFailableResult&&) = default;
	TFailableResult& operator=(TFailableResult&&) = default;

	explicit operator bool() const
	{
		return Succeeded();
	}

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
			Ret.Add(UErrorReportingError::InvalidObject());
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
	requires CAwaitable<AwaitableType>;
	{ Awaitable.await_resume() } -> CInstantiationOf<TFailableResult>;
};


template <typename AwaitableType>
concept CNonErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	requires CAwaitable<AwaitableType>;
	requires !CErrorReportingAwaitable<AwaitableType>;
};


template <CErrorReportingAwaitable AwaitableType>
struct TErrorReportResultType
{
	using Type = typename decltype(std::declval<AwaitableType>().await_resume())::ResultType;
};


template <CErrorReportingAwaitable AwaitableType>
class TErrorRemovedAwaitable
{
public:
	using ResultType = typename TErrorReportResultType<AwaitableType>::Type;
	
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
	bool await_suspend(HandleType&& Handle, std::source_location Current = std::source_location::current())
	{
		SourceLocation = Current;

		if (Awaitable.await_ready())
		{
			Result = Awaitable.await_resume();
			if (Result->Failed())
			{
				Handle.promise().SetErrors(Result->GetErrors());
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

				if (This->Result->Failed())
				{
					Handle.promise().SetErrors(This->Result->GetErrors());
					Handle.destroy();
				}
				else
				{
					Handle.resume();
				}
			}

			void destroy() const { Handle.destroy(); }
		};

		// 지금 이 구현 이게 가정되어 있음 여기 걸리면 구현 수정해야 됨
		static_assert(std::is_void_v<decltype(Awaitable.await_suspend(std::declval<FFailChecker>()))>);
		
		Awaitable.await_suspend(FFailChecker{this, Forward<HandleType>(Handle)});
		return true;
	}

	ResultType await_resume()
	{
		return MoveTempIfPossible(Result->GetResult());
	}

private:
	AwaitableType Awaitable;
	TOptional<TFailableResult<ResultType>> Result;
	std::source_location SourceLocation;
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
	auto await_suspend(HandleType&& Handle)
	{
		struct FAbortChecker
		{
			TAlwaysResumingAwaitable* This;
			std::decay_t<HandleType> Handle;

			void resume() const { Handle.resume(); }

			void destroy() const
			{
				This->bAborted = true;
				Handle.resume();
			}
		};

		bAborted = false;
		return Awaitable.await_suspend(FAbortChecker{this, Forward<HandleType>(Handle)});
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


template <typename AwaitableType>
struct TEnsureErrorReporting
{
	using Type = TAlwaysResumingAwaitable<AwaitableType>;
};

template <CErrorReportingAwaitable AwaitableType>
struct TEnsureErrorReporting<AwaitableType>
{
	using Type = AwaitableType;
};