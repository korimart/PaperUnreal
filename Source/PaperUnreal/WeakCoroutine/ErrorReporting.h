// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AwaitableAdaptor.h"
#include "ConditionalResumeAwaitable.h"
#include "TypeTraits.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
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
class UInvalidObjectError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UInvalidObjectError* InvalidObject()
	{
		return NewError<UInvalidObjectError>(TEXT("TFailable에 저장된 UObject가 Garbage임"
			"(애초에 쓰레기로 초기화 되었을 수도 있고 도중에 쓰레기가 되었을 수도 있음. "
			"하지만 TFailable이 Strong Object Pointer를 유지하기 때문에 레퍼런스 소실에 의한 것이 아니라 "
			"누군가가 명시적으로 쓰레기로 만든 것임)"));
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


template <CUObjectUnsafeWrapper ResultType>
struct TFailableResultStorage<ResultType>
{
	TFailableResultStorage(const ResultType& Value)
		: Result(Value), StrongResult(TUObjectUnsafeWrapperTypeTraits<ResultType>::GetUObject(Value))
	{
		if (StrongResult.Get() == nullptr)
		{
			bInitializedWithNull = true;
		}
	}
	
	TFailableResultStorage(ResultType&& Value)
		: Result(MoveTemp(Value)), StrongResult(TUObjectUnsafeWrapperTypeTraits<ResultType>::GetUObject(Value))
	{
		if (StrongResult.Get() == nullptr)
		{
			bInitializedWithNull = true;
		}
	}

	bool Expired() const { return !bInitializedWithNull && !::IsValid(StrongResult.Get()); }
	
	ResultType& Get() { return Result; }
	const ResultType& Get() const { return Result; }

private:
	bool bInitializedWithNull = false;
	ResultType Result;
	TStrongObjectPtr<UObject> StrongResult;
};


template <typename InResultType>
class TFailableResult
{
public:
	using ResultType = InResultType;

	/**
	 * 이 클래스는 현재 결과 or 에러 중 하나가 반드시 설정되어 있음을 가정하고 있음
	 * 둘 다 없이 초기화에 성공하면 멤버 함수들 고장남
	 */
	TFailableResult() = delete;

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

	TFailableResult(const TFailableResult&) = delete;
	TFailableResult& operator=(const TFailableResult&) = delete;

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
			Ret.Add(UInvalidObjectError::InvalidObject());
		}
		Algo::Transform(Errors, Ret, [](auto& Each) { return Each.Get(); });
		return Ret;
	}

	template <typename... ErrorTypes>
	bool OnlyContains() const
	{
		const TArray<UClass*> AllowedUClasses{ErrorTypes::StaticClass()...};
		for (const auto& Error : Errors)
		{
			if (Algo::AllOf(AllowedUClasses, [&](UClass* AllowedUClass) { return !Error->IsA(AllowedUClass); }))
			{
				return false;
			}
		}
		return true;
	}
	
	template <typename... ErrorTypes>
	bool ContainsAnyOf() const
	{
		const TArray<UClass*> WantedUClasses{ErrorTypes::StaticClass()...};
		for (const auto& Error : Errors)
		{
			if (Algo::AnyOf(WantedUClasses, [&](UClass* WantedUClass) { return Error->IsA(WantedUClass); }))
			{
				return true;
			}
		}
		return false;
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


template <typename AwaitableType>
struct TErrorReportResultType
{
	using Type = typename decltype(std::declval<AwaitableType>().await_resume())::ResultType;
};


template <CErrorReportingAwaitable InnerAwaitableType>
class TAbortIfErrorAwaitable
	: public TConditionalResumeAwaitable<TAbortIfErrorAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortIfErrorAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	template <typename AwaitableType>
	TAbortIfErrorAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CNonErrorReportingAwaitable<TAbortIfErrorAwaitable>);
	}

	bool ShouldResume(const auto& Handle, const TFailableResult<ResultType>& Result) const
	{
		if (Result.Failed())
		{
			Handle.promise().SetErrors(Result.GetErrors());
			return false;
		}

		return true;
	}

	ResultType await_resume()
	{
		return Super::await_resume().GetResult();
	}
};

template <typename AwaitableType>
TAbortIfErrorAwaitable(AwaitableType&&) -> TAbortIfErrorAwaitable<AwaitableType>;


struct FAbortIfErrorAdaptor : TAwaitableAdaptorBase<FAbortIfErrorAdaptor>
{
	template <CAwaitable AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, FAbortIfErrorAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TAbortIfErrorAwaitable{Forward<AwaitableType>(Awaitable)};
		}
	}
};


template <CErrorReportingAwaitable InnerAwaitableType, typename AllowedErrorTypeList>
class TAbortIfErrorNotInAwaitable
	: public TConditionalResumeAwaitable<TAbortIfErrorNotInAwaitable<InnerAwaitableType, AllowedErrorTypeList>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TAbortIfErrorNotInAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	static constexpr bool bAllowAll = std::is_same_v<void, AllowedErrorTypeList>;

	template <typename AwaitableType>
	TAbortIfErrorNotInAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TAbortIfErrorNotInAwaitable>);
	}

	bool ShouldResume(const auto& Handle, const TFailableResult<ResultType>& Result) const
	{
		if (Result.Succeeded())
		{
			return true;
		}

		if constexpr (!bAllowAll)
		{
			if ([&]<typename... AllowedErrorTypes>(TTypeList<AllowedErrorTypes...>)
			{
				return !Result.template OnlyContains<AllowedErrorTypes...>();
			}(AllowedErrorTypeList{}))
			{
				Handle.promise().SetErrors(Result.GetErrors());
				return false;
			}
		}

		return true;
	}
};


template <typename AllowedErrorTypeList>
struct TAbortIfErrorNotInAdaptor : TAwaitableAdaptorBase<TAbortIfErrorNotInAdaptor<AllowedErrorTypeList>>
{
	template <typename AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, TAbortIfErrorNotInAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TAbortIfErrorNotInAwaitable<AwaitableType, AllowedErrorTypeList>{Forward<AwaitableType>(Awaitable)};
		}
	}
};


namespace Awaitables
{
	inline FAbortIfErrorAdaptor AbortIfError()
	{
		return {};
	}

	template <typename AllowedErrorTypeList>
	TAbortIfErrorNotInAdaptor<AllowedErrorTypeList> AbortIfErrorNotIn()
	{
		return {};
	}
}