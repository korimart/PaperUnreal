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


/**
 * TOptional과 비슷하지만 값의 유무와 더불어 왜 값을 설정할 수 없었는지를 설명하는 에러를 세팅할 수 있는 클래스
 * 
 * 두 가지 상태를 가질 수 있습니다. 1) 값을 가짐 2) 에러가 발생하여 값을 가지지 않고 에러를 조회할 수 있음
 *
 * UObject*, TScriptInterface, 언리얼 인터페이스 클래스와 같이 UPROPERTY의 도움 없이는 dangling pointer가
 * 될 수 있는 객체들에 대해서는 TStrongObjectPtr로 감싸 값을 조회할 때까지 생명주기를 연장합니다.
 * @see UInvalidObjectError
 */
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

	/**
	 * if (FailableResult) {} 신택스 지원용
	 */
	explicit operator bool() const
	{
		return Succeeded();
	}

	/**
	 * 필요한 경우 에러가 발생한 TFailableResult를 다른 곳에 넘기기 전에
	 * 내 에러를 추가하여 에러 발생 원인에 대해 좀 더 자세한 설명을 제공할 수 있습니다.
	 *
	 * 예시) 정수 나누기 함수가 TFailableResult<int32>에 UDivideByZeroError를 실어서 반환한 경우
	 * 정수 나누기 함수를 호출한 함수가 대미지 계산 함수이면 여기에 추가적으로 UInvalidDamageError를 더해서
	 * 자신의 호출자에게 넘길 수 있습니다.
	 */
	void AddError(UFailableResultError* Error)
	{
		Errors.Emplace(Error);
	}

	/**
	 * 에러가 발생해 GetResult를 호출할 수 없음을 의미합니다.
	 */
	bool Failed() const
	{
		return (Result.IsSet() && Result->Expired()) || Errors.Num() > 0;
	}

	/**
	 * 값이 성공적으로 설정돼 GetResult를 호출할 수 있음을 의미합니다.
	 */
	bool Succeeded() const
	{
		return !Failed();
	}

	/**
	 * 값을 레퍼런스로 반환합니다. Succeeded()가 참일 때만 호출 가능
	 */
	decltype(auto) GetResult()
	{
		check(Succeeded());
		return Result->Get();
	}

	/**
	 * 값을 const 레퍼런스로 반환합니다. Succeeded()가 참일 때만 호출 가능
	 */
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

	/**
	 * 발생한 에러들이 ErrorTypes의 부분집합이면 true를 반환합니다.
	 */
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

	/**
	 * 발생한 에러 중에서 하나라도 ErrorTypes에 속하는 것이 있으면 true를 반환합니다.
	 */
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


/**
 * Awaitable에 co_await을 한 결과가 TFailableResult이면 CErrorReportingAwaitable
 */
template <typename AwaitableType>
concept CErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	requires CAwaitable<AwaitableType>;
	{ Awaitable.await_resume() } -> CInstantiationOf<TFailableResult>;
};


/**
 * Awaitable에 co_await을 한 결과가 TFailableResult가 아니면 CNonErrorReportingAwaitable
 */
template <typename AwaitableType>
concept CNonErrorReportingAwaitable = requires(AwaitableType Awaitable)
{
	requires CAwaitable<AwaitableType>;
	requires !CErrorReportingAwaitable<AwaitableType>;
};


/**
 * Error Reporting Awaitable이 co_await으로 반환하는 TFailableResult<T>의 T를 반환하는 Type Function
 */
template <typename AwaitableType>
struct TErrorReportResultType
{
	using Type = typename decltype(std::declval<AwaitableType>().await_resume())::ResultType;
};


/**
 * @see Awaitables::DestroyIfError
 */
template <CErrorReportingAwaitable InnerAwaitableType>
class TDestroyIfErrorAwaitable
	: public TConditionalResumeAwaitable<TDestroyIfErrorAwaitable<InnerAwaitableType>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TDestroyIfErrorAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	template <typename AwaitableType>
	TDestroyIfErrorAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CNonErrorReportingAwaitable<TDestroyIfErrorAwaitable>);
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
TDestroyIfErrorAwaitable(AwaitableType&&) -> TDestroyIfErrorAwaitable<AwaitableType>;


struct FDestroyIfErrorAdaptor : TAwaitableAdaptorBase<FDestroyIfErrorAdaptor>
{
	template <CAwaitable AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, FDestroyIfErrorAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TDestroyIfErrorAwaitable{Forward<AwaitableType>(Awaitable)};
		}
	}
};


/**
 * @see Awaitables::DestroyIfErrorNotIn
 */
template <CErrorReportingAwaitable InnerAwaitableType, typename AllowedErrorTypeList>
class TDestroyIfErrorNotInAwaitable
	: public TConditionalResumeAwaitable<TDestroyIfErrorNotInAwaitable<InnerAwaitableType, AllowedErrorTypeList>, InnerAwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TDestroyIfErrorNotInAwaitable, InnerAwaitableType>;
	using ResultType = typename TErrorReportResultType<InnerAwaitableType>::Type;

	static constexpr bool bAllowAll = std::is_same_v<void, AllowedErrorTypeList>;

	template <typename AwaitableType>
	TDestroyIfErrorNotInAwaitable(AwaitableType&& Awaitable)
		: Super(Forward<AwaitableType>(Awaitable))
	{
		static_assert(CErrorReportingAwaitable<TDestroyIfErrorNotInAwaitable>);
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
struct TDestroyIfErrorNotInAdaptor : TAwaitableAdaptorBase<TDestroyIfErrorNotInAdaptor<AllowedErrorTypeList>>
{
	template <typename AwaitableType>
	friend decltype(auto) operator|(AwaitableType&& Awaitable, TDestroyIfErrorNotInAdaptor)
	{
		if constexpr (!CErrorReportingAwaitable<std::decay_t<AwaitableType>>)
		{
			return Forward<AwaitableType>(Awaitable);
		}
		else
		{
			return TDestroyIfErrorNotInAwaitable<AwaitableType, AllowedErrorTypeList>{Forward<AwaitableType>(Awaitable)};
		}
	}
};


namespace Awaitables
{
	/**
	 * Awaitable을 받아서 Awaitable에 co_await을 호출한 결과가 에러 상태인 TFailableResult이면
	 * 코루틴 핸들에 destroy를 호출하는 Awaitable을 반환합니다.
	 *
	 * co_await를 호출한 결과가 Succeeded 상태인 TFailableResult<T>이면 T를 반환합니다.
	 * (즉 TFailableResult를 제거해서 반환)
	 * 
	 * Awaitable이 Error Reporting Awaitable이 아니면 no-op입니다.
	 * @see CErrorReportingAwaitable
	 */
	inline FDestroyIfErrorAdaptor DestroyIfError()
	{
		return {};
	}

	/**
	 * Awaitable을 받아서 Awaitable에 co_await을 호출한 결과가 에러 상태인 TFailableResult이고
	 * 발생한 에러가 AllowedErrorTypeList 중의 하나가 아니면 코루틴 핸들에 destroy를 호출하는 Awaitable을 반환합니다.
	 *
	 * DestroyIfError와 달리 항상 TFailableResult를 반환합니다.
	 * 
	 * Awaitable이 Error Reporting Awaitable이 아니면 no-op입니다.
	 * @see CErrorReportingAwaitable
	 */
	template <typename AllowedErrorTypeList>
	TDestroyIfErrorNotInAdaptor<AllowedErrorTypeList> DestroyIfErrorNotIn()
	{
		return {};
	}
}