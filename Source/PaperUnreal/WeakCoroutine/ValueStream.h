// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FilterAwaitable.h"
#include "TransformAwaitable.h"
#include "CancellableFuture.h"
#include "MinimalCoroutine.h"
#include "NoDestroyAwaitable.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "ValueStream.generated.h"


UCLASS()
class UEndOfStreamError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UEndOfStreamError* NewError()
	{
		return ::NewError<UEndOfStreamError>(TEXT("스트림의 끝에 도달했습니다."));
	}
};


template <typename T>
class TValueStreamValueReceiver
{
public:
	using ValueType = T;

	TValueStreamValueReceiver() = default;

	TValueStreamValueReceiver(const TValueStreamValueReceiver&) = delete;
	TValueStreamValueReceiver& operator=(const TValueStreamValueReceiver&) = delete;

	TValueStreamValueReceiver(TValueStreamValueReceiver&&) = default;
	TValueStreamValueReceiver& operator=(TValueStreamValueReceiver&&) = default;

	TCancellableFuture<T> NextValue()
	{
		if (UnclaimedFutures.Num() > 0)
		{
			return PopFront(UnclaimedFutures);
		}

		if (bClosed)
		{
			return UEndOfStreamError::NewError();
		}

		auto [Promise, Future] = MakePromise<T>();
		Promises.Add(MoveTemp(Promise));
		return MoveTemp(Future);
	}

	template <typename U>
	void ReceiveValue(U&& Value)
	{
		check(!bClosed);

		if (Promises.Num() == 0)
		{
			UnclaimedFutures.Add(TCancellableFuture<T>{Forward<U>(Value)});
			return;
		}

		PopFront(Promises).SetValue(Forward<U>(Value));
	}

	void Close()
	{
		bClosed = true;

		while (Promises.Num() > 0)
		{
			PopFront(Promises).SetValue(UEndOfStreamError::NewError());
		}
	}

	bool IsClosed() const
	{
		return bClosed;
	}

private:
	bool bClosed = false;
	TArray<TCancellableFuture<T>> UnclaimedFutures;
	TArray<TCancellablePromise<T>> Promises;
};


template <typename T>
class TValueStream
{
public:
	using ReceiverType = TValueStreamValueReceiver<T>;
	using ValueType = T;

	TValueStream() = default;

	// 복사 허용하면 내가 Next하고 다른 애가 Next하면 다른 애의 Next는 나 다음에 호출되므로
	// 사실상 Next가 아니라 NextNext임 이게 좀 혼란스러울 수 있으므로 일단 막음
	TValueStream(const TValueStream&) = delete;
	TValueStream& operator=(const TValueStream&) = delete;

	TValueStream(TValueStream&&) = default;
	TValueStream& operator=(TValueStream&&) = default;

	TWeakPtr<ReceiverType> GetReceiver() const
	{
		return Receiver;
	}

	friend auto operator co_await(const TValueStream& Stream)
	{
		return operator co_await(Stream.Receiver->NextValue());
	}

	auto EndOfStream()
	{
		return *this
			| Awaitables::FilterWithError([](const TFailableResult<ValueType>& Result)
			{
				return !Result;
			})
			| Awaitables::TransformWithError([](const TFailableResult<ValueType>& Result)
			{
				if (Result.template OnlyContains<UEndOfStreamError>())
				{
					return TFailableResult{std::monostate{}};
				}
				return TFailableResult<std::monostate>{Result.GetErrors()};
			});
	}

private:
	TSharedPtr<ReceiverType> Receiver = MakeShared<ReceiverType>();
};


// TODO cancellablefuture.h에 있는 delegate 파라미터 가져오는 로직 분리해서 여기도 T 넘길 필요 없도록 사용법 개선
template <typename T>
TTuple<TValueStream<T>, FDelegateHandle> MakeStreamFromDelegate(
	auto& MulticastDelegate, const TArray<T>& ReadyValues = {})
{
	return MakeStreamFromDelegate(MulticastDelegate, ReadyValues,
		[](auto...) { return true; },
		[]<typename ArgType>(ArgType&& Arg) -> decltype(auto) { return Forward<ArgType>(Arg); });
}


/**
 * 주의 : 델리게이트를 복사하는 경우 여러 델리게이트를 통해 Value Stream에 Value가 공급될 수 있음
 * 
 * @tparam T
 * @tparam DelegateType 
 * @tparam PredicateType 
 * @tparam TransformFuncType 
 * @param MulticastDelegate 
 * @param ReadyValues 
 * @param Predicate 
 * @param TransformFunc 
 * @return 
 */
template <typename T, typename DelegateType, typename PredicateType, typename TransformFuncType>
TTuple<TValueStream<T>, FDelegateHandle> MakeStreamFromDelegate(
	DelegateType& MulticastDelegate,
	const TArray<T>& ReadyValues,
	PredicateType&& Predicate,
	TransformFuncType&& TransformFunc)
{
	TValueStream<T> Ret;

	auto Receiver = Ret.GetReceiver();
	for (const T& Each : ReadyValues)
	{
		Receiver.Pin()->ReceiveValue(Each);
	}

	struct FStreamCloser
	{
		TWeakPtr<TValueStreamValueReceiver<T>> Receiver;

		~FStreamCloser()
		{
			if (Receiver.IsValid())
			{
				Receiver.Pin()->Close();
			}
		}
	};

	FDelegateHandle Handle = MulticastDelegate.Add(DelegateType::FDelegate::CreateSPLambda(Receiver.Pin().ToSharedRef(),
		[
			Receiver,
			Predicate = Forward<PredicateType>(Predicate),
			TransformFunc = Forward<TransformFuncType>(TransformFunc),
			StreamCloser = MakeShared<FStreamCloser>(Receiver)
		]<typename... ArgTypes>(ArgTypes&&... NewValues)
		{
			if (Predicate(Forward<ArgTypes>(NewValues)...))
			{
				Receiver.Pin()->ReceiveValue(TransformFunc(Forward<ArgTypes>(NewValues)...));
			}
		}));

	return MakeTuple(MoveTemp(Ret), Handle);
}


namespace Stream_Private
{
	template <int32 Index>
	FMinimalCoroutine CombineImpl(auto NoDestroyAwaitable, auto WeakReceiver, auto SharedTuple)
	{
		using ReceiverValueType = typename std::decay_t<decltype(*WeakReceiver.Pin())>::ValueType;

		while (true)
		{
			auto FailableResult = co_await NoDestroyAwaitable;

			// Combined Value Stream이 존재하지 않거나 종료된 경우 더 이상 값을 공급할 필요가 없음
			// 이 코루틴을 종료한다 (값을 공급하는 다른 코루틴들은 자신에게 CPU 타임이 왔을 때 여기에 닿으면서 또한 종료)
			if (!WeakReceiver.IsValid() || WeakReceiver.Pin()->IsClosed())
			{
				co_return;
			}
			
			// 1. UNoDestroyError: Awaitable이 종료를 요청한 경우 이 코루틴은 더 이상 값을 공급할 수 없음
			// 그러므로 Combined Value Stream도 새 값을 만들 수 없으므로 닫고 코루틴도 종료한다.
			//
			// 2. UEndOfStreamError: 만약에 기다리던 것이 Value Stream이었으면 무한대로
			// UEndOfStreamError를 내뱉으므로 Combined Value Stream에 새 값을 공급하는 것이 불가능
			if (FailableResult.template ContainsAnyOf<UNoDestroyError, UEndOfStreamError>())
			{
				WeakReceiver.Pin()->Close();
				co_return;
			}

			SharedTuple->template Get<Index>().Emplace(MoveTemp(FailableResult));

			const bool bContainsUnset = SharedTuple->ApplyAfter([&](const auto&... OptionalFailableResults)
			{
				return (!OptionalFailableResults.IsSet() || ...);
			});

			// 값을 공급하는 다른 코루틴이 아직 한 번도 값을 공급하지 않았으면 Combined Value Stream에도 공급할 값이 없음
			if (bContainsUnset)
			{
				continue;
			}

			TArray<UFailableResultError*> Errors;
			SharedTuple->ApplyAfter([&](const auto&... OptionalFailableResults)
			{
				(Errors.Append(OptionalFailableResults->GetErrors()), ...);
			});

			// 여러 공급자 중에서 에러를 발생시킨 코루틴이 하나라도 있으면 값을 합칠 수 없으므로
			// 모든 에러를 긁어모아서 Combined Value Stream에 전달한다
			if (Errors.Num() > 0)
			{
				WeakReceiver.Pin()->ReceiveValue(TFailableResult<ReceiverValueType>{MoveTemp(Errors)});
				continue;
			}

			auto CombinedValue = SharedTuple->ApplyAfter([&](const auto&... OptionalFailableResults)
			{
				return ReceiverValueType{OptionalFailableResults->GetResult()...};
			});

			WeakReceiver.Pin()->ReceiveValue(TFailableResult<ReceiverValueType>{MoveTemp(CombinedValue)});
		}
	}
}


namespace Stream
{
	// TODO live data, multicast delegate를 받는 버전 추가
	template <typename... AwaitableTypes>
	auto Combine(AwaitableTypes&&... Awaitables)
	{
		// 이 함수의 원리는 다음과 같음
		// 각 Awaitable에 대해 FMinimalCoroutine을 실행하여 co_await을 하면서 값을 가져온다
		// (Awaitable이 3개면 FMinimalCoroutine을 세 개 실행)
		// 세 값이 모두 준비되면 TTuple로 합쳐서 반환값인 Combined Value Stream으로 전달
		// 만약 세 값 중 하나라도 에러가 발생하면 에러로 전달
		// 세 코루틴 중 하나라도 종료되면 Combined Value Stream도 닫는다 (UEndOfStreamError)

		// NoDestroy를 붙이면 await_resume의 반환이 항상 TFailableResult<ResultType>이 되기 때문에 ResultType을 취할 수 있음
		using RetTupleType = TTuple<
			typename decltype((Forward<AwaitableTypes>(Awaitables) | Awaitables::NoDestroy()).await_resume())::ResultType...>;

		using OptionalFailableTupleType = TTuple<
			TOptional<decltype((Forward<AwaitableTypes>(Awaitables) | Awaitables::NoDestroy()).await_resume())>...>;

		TValueStream<RetTupleType> Ret;

		auto SharedTuple = MakeShared<OptionalFailableTupleType>();
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>)
		{
			(Stream_Private::CombineImpl<Indices>(Forward<AwaitableTypes>(Awaitables) | Awaitables::NoDestroy(), Ret.GetReceiver(), SharedTuple), ...);
		}(std::index_sequence_for<AwaitableTypes...>{});

		return Ret;
	}

	template <typename... AwaitableTypes>
	auto AllOf(AwaitableTypes&&... Awaitables)
	{
		return Combine(Forward<AwaitableTypes>(Awaitables)...)
			| Awaitables::Transform([]<typename... BoolTypes>(BoolTypes... bBools){ return (bBools && ...); })
			| Awaitables::FilterDuplicate<bool>();
	}
}
