// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <source_location>

#include "CoreMinimal.h"
#include "ErrorReporting.h"


struct FLoggingPromise
{
	~FLoggingPromise()
	{
		if (LastCoAwaitSourceLocation && false)
		{
			UE_LOG(LogTemp, Warning, TEXT("FLoggingPromise 에러 발생."));
			UE_LOG(LogTemp, Warning, TEXT("파일: %hs"), LastCoAwaitSourceLocation->file_name());
			UE_LOG(LogTemp, Warning, TEXT("함수: %hs"), LastCoAwaitSourceLocation->function_name());
			UE_LOG(LogTemp, Warning, TEXT("라인: %d"), LastCoAwaitSourceLocation->line());
			UE_LOG(LogTemp, Warning, TEXT("사유는 다음과 같습니다."));

			int32 Index = 0;
			for (UFailableResultError* Each : Errors)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%d] %s: %s"), Index, *Each->GetClass()->GetName(), *Each->What);
				Index++;
			}
		}
	}
	
	void SetSourceLocation(std::source_location SL)
	{
		LastCoAwaitSourceLocation = SL;
	}

	void SetErrors(const TArray<UFailableResultError*>& InErrors)
	{
		Errors = InErrors;
	}

	void AddError(UFailableResultError* Error)
	{
		Errors.Add(Error);
	}

	void ClearErrors()
	{
		LastCoAwaitSourceLocation.Reset();
		Errors.Empty();
	}

private:
	TOptional<std::source_location> LastCoAwaitSourceLocation;

	// 설정된 이후 즉시 코루틴이 파괴된다고 가정해 UObject의 생명주기에 관해 신경쓰지 않음
	TArray<UFailableResultError*> Errors;
};


template <typename InnerAwaitableType>
class TCaptureSourceLocationAwaitable
{
public:
	template <typename AwaitableType>
	TCaptureSourceLocationAwaitable(AwaitableType&& Awaitable)
		: InnerAwaitable(Forward<AwaitableType>(Awaitable))
	{
	}
	
	bool await_ready() const
	{
		return InnerAwaitable.await_ready();
	}

	template <typename HandleType>
	auto await_suspend(HandleType&& Handle, std::source_location SL = std::source_location::current())
	{
		Handle.promise().SetSourceLocation(SL);
		return InnerAwaitable.await_suspend(Forward<HandleType>(Handle));
	}

	auto await_resume()
	{
		return InnerAwaitable.await_resume();
	}
	
private:
	InnerAwaitableType InnerAwaitable;
};

template <typename AwaitableType>
TCaptureSourceLocationAwaitable(AwaitableType&&) -> TCaptureSourceLocationAwaitable<AwaitableType>;


struct FCaptureSourceLocationAdaptor : TAwaitableAdaptorBase<FCaptureSourceLocationAdaptor>
{
	template <typename AwaitableType>
	friend auto operator|(AwaitableType&& Awaitable, FCaptureSourceLocationAdaptor)
	{
		return TCaptureSourceLocationAwaitable{Forward<AwaitableType>(Awaitable)};
	}
};


namespace Awaitables
{
	inline FCaptureSourceLocationAdaptor CaptureSourceLocation()
	{
		return {};
	}
}