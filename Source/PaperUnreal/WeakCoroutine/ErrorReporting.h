// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
			Ret.Add(UErrorReportingError::InvalidObject());
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
	bool ContainsAny() const
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


template <CErrorReportingAwaitable AwaitableType>
struct TErrorReportResultType
{
	using Type = typename decltype(std::declval<AwaitableType>().await_resume())::ResultType;
};


template <typename Derived, CAwaitable AwaitableType>
class TConditionalResumeAwaitable
{
public:
	using ReturnType = decltype(std::declval<AwaitableType>().await_resume());

	TConditionalResumeAwaitable(AwaitableType&& Inner)
		: Awaitable(MoveTemp(Inner))
	{
	}

	TConditionalResumeAwaitable(const TConditionalResumeAwaitable&) = delete;
	TConditionalResumeAwaitable& operator=(const TConditionalResumeAwaitable&) = delete;

	TConditionalResumeAwaitable(TConditionalResumeAwaitable&&) = default;
	TConditionalResumeAwaitable& operator=(TConditionalResumeAwaitable&&) = default;

	bool await_ready() const
	{
		return false;
	}

	template <typename HandleType>
	bool await_suspend(const HandleType& Handle)
	{
		if (Awaitable.await_ready())
		{
			const bool bResume = ShouldResume(Handle);
			if (!bResume)
			{
				Handle.destroy();
				return true;
			}
			return false;
		}

		struct FConditionalResumeHandle
		{
			TConditionalResumeAwaitable* This;
			HandleType Handle;

			void resume() const
			{
				const bool bResume = This->ShouldResume(Handle);
				bResume ? Handle.resume() : Handle.destroy();
			}

			void destroy() const { Handle.destroy(); }
		};

		using SuspendType = decltype(std::declval<AwaitableType>().await_suspend(std::declval<FConditionalResumeHandle>()));

		if constexpr (std::is_void_v<SuspendType>)
		{
			Awaitable.await_suspend(FConditionalResumeHandle{this, Handle});
			return true;
		}
		else if (Awaitable.await_suspend(FConditionalResumeHandle{this, Handle}))
		{
			return true;
		}

		const bool bResume = ShouldResume(Handle);
		if (!bResume)
		{
			Handle.destroy();
			return true;
		}
		return false;
	}

private:
	AwaitableType Awaitable;

	template <typename HandleType>
	bool ShouldResume(const HandleType& Handle)
	{
		struct FReadOnlyHandle
		{
			FReadOnlyHandle(HandleType InHandle)
				: Handle(InHandle)
			{
			}

			auto& promise() const
			{
				return Handle.promise();
			}

		private:
			HandleType Handle;
		};

		Derived* AsDerived = static_cast<Derived*>(this);
		return AsDerived->ShouldResume(FReadOnlyHandle{Handle}, Awaitable.await_resume());
	}
};


template <CErrorReportingAwaitable AwaitableType>
class TErrorRemovingAwaitable : public TConditionalResumeAwaitable<TErrorRemovingAwaitable<AwaitableType>, AwaitableType>
{
public:
	using Super = TConditionalResumeAwaitable<TErrorRemovingAwaitable, AwaitableType>;
	using ResultType = typename TErrorReportResultType<AwaitableType>::Type;

	TErrorRemovingAwaitable(AwaitableType&& Inner)
		: Super(MoveTemp(Inner))
	{
		static_assert(CNonErrorReportingAwaitable<TErrorRemovingAwaitable>);
	}

	TErrorRemovingAwaitable(const TErrorRemovingAwaitable&) = delete;
	TErrorRemovingAwaitable& operator=(const TErrorRemovingAwaitable&) = delete;

	TErrorRemovingAwaitable(TErrorRemovingAwaitable&&) = default;
	TErrorRemovingAwaitable& operator=(TErrorRemovingAwaitable&&) = default;

	bool ShouldResume(const auto& Handle, TFailableResult<ResultType>&& InnerResult)
	{
		Result = MoveTemp(InnerResult);

		if (Result->Failed())
		{
			Handle.promise().SetErrors(Result->GetErrors());
			return false;
		}

		return true;
	}

	ResultType await_resume()
	{
		return MoveTempIfPossible(Result->GetResult());
	}

private:
	TOptional<TFailableResult<ResultType>> Result;
};


struct FAllowAll
{
};


template <CErrorReportingAwaitable AwaitableType, typename AllowedErrorTypeList, typename NeverAllowedTypeList>
class TErrorFilteringAwaitable
	: public TConditionalResumeAwaitable
	<
		TErrorFilteringAwaitable<AwaitableType, AllowedErrorTypeList, NeverAllowedTypeList>,
		AwaitableType
	>
{
public:
	using Super = TConditionalResumeAwaitable<TErrorFilteringAwaitable, AwaitableType>;
	using ResultType = typename TErrorReportResultType<AwaitableType>::Type;

	static constexpr bool bAllowAll = std::is_same_v<FAllowAll, AllowedErrorTypeList>;

	TErrorFilteringAwaitable(AwaitableType&& Inner)
		: Super(MoveTemp(Inner))
	{
		static_assert(CErrorReportingAwaitable<TErrorFilteringAwaitable>);
	}

	TErrorFilteringAwaitable(const TErrorFilteringAwaitable&) = delete;
	TErrorFilteringAwaitable& operator=(const TErrorFilteringAwaitable&) = delete;

	TErrorFilteringAwaitable(TErrorFilteringAwaitable&&) = default;
	TErrorFilteringAwaitable& operator=(TErrorFilteringAwaitable&&) = default;

	bool ShouldResume(const auto& Handle, TFailableResult<ResultType>&& InnerResult)
	{
		Result = MoveTemp(InnerResult);

		if (Result->Succeeded())
		{
			return true;
		}

		if constexpr (!bAllowAll)
		{
			if ([&]<typename... AllowedErrorTypes>(TTypeList<AllowedErrorTypes...>)
			{
				return !Result->template OnlyContains<AllowedErrorTypes...>();
			}(AllowedErrorTypeList{}))
			{
				Handle.promise().SetErrors(Result->GetErrors());
				return false;
			}
		}

		if ([&]<typename... NeverAllowedErrorTypes>(TTypeList<NeverAllowedErrorTypes...>)
		{
			return Result->template ContainsAny<NeverAllowedErrorTypes...>();
		}(NeverAllowedTypeList{}))
		{
			Handle.promise().SetErrors(Result->GetErrors());
			return false;
		}

		return true;
	}

	TFailableResult<ResultType> await_resume()
	{
		return MoveTemp(*Result);
	}

private:
	TOptional<TFailableResult<ResultType>> Result;
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
