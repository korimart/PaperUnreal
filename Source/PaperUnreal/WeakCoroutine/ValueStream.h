// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FilterAwaitable.h"
#include "TransformAwaitable.h"
#include "CancellableFuture.h"
#include "MinimalCoroutine.h"
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
			| Awaitables::Filter([](const TFailableResult<ValueType>& Result)
			{
				return Result.template ContainsAnyOf<UEndOfStreamError>();
			})
			| Awaitables::Transform([](const TFailableResult<ValueType>&)
			{
				// TODO TFailalbeResult<void>가 가능해지면 return;으로 수정
				return std::monostate{};
			});
	}

private:
	TSharedPtr<ReceiverType> Receiver = MakeShared<ReceiverType>();
};


namespace Stream
{
	// TODO cancellablefuture.h에 있는 delegate 파라미터 가져오는 로직 분리해서 여기도 T 넘길 필요 없도록 사용법 개선
	template <typename T>
	TTuple<TValueStream<T>, FDelegateHandle> MakeFromMulticastDelegate(
		auto& MulticastDelegate, const TArray<T>& ReadyValues = {})
	{
		return MakeFromMulticastDelegate(MulticastDelegate, ReadyValues,
			[](auto...) { return true; },
			[]<typename ArgType>(ArgType&& Arg) -> decltype(auto) { return Forward<ArgType>(Arg); });
	}


	template <typename T, typename DelegateType, typename PredicateType, typename TransformFuncType>
	TTuple<TValueStream<T>, FDelegateHandle> MakeFromMulticastDelegate(
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

		struct FCopyChecker
		{
			TWeakPtr<TValueStreamValueReceiver<T>> Receiver;

			FCopyChecker(TWeakPtr<TValueStreamValueReceiver<T>> InReceiver)
				: Receiver(InReceiver)
			{
			}

			FCopyChecker(const FCopyChecker&)
			{
				AssertNoCopy();
			}

			FCopyChecker& operator=(const FCopyChecker&)
			{
				AssertNoCopy();
				return *this;
			}

			FCopyChecker(FCopyChecker&&) = default;
			FCopyChecker& operator=(FCopyChecker&&) = default;

			~FCopyChecker()
			{
				if (auto Pinned = Receiver.Pin())
				{
					Pinned->Close();
				}
			}

			// MulticastDelegate를 복사하는 경우 여러 곳에서 Receiver에 값을 넣을 수 있게 됨
			// 이것에 대한 처리를 구현하지 않았으므로 실수로 델리게이트를 복사하지 않도록 막아둠
			void AssertNoCopy() { check(false); }
		};

		FDelegateHandle Handle = MulticastDelegate.Add(DelegateType::FDelegate::CreateSPLambda(Receiver.Pin().ToSharedRef(),
			[
				Receiver,
				Predicate = Forward<PredicateType>(Predicate),
				TransformFunc = Forward<TransformFuncType>(TransformFunc),
				CopyChecker = FCopyChecker{Receiver}
			]<typename... ArgTypes>(ArgTypes&&... NewValues)
			{
				if (Predicate(Forward<ArgTypes>(NewValues)...))
				{
					Receiver.Pin()->ReceiveValue(TransformFunc(Forward<ArgTypes>(NewValues)...));
				}
			}));

		return MakeTuple(MoveTemp(Ret), Handle);
	}

	
	template <int32 Index>
	FMinimalCoroutine CombineImpl(auto ValueStream, auto WeakReceiver, auto SharedTuple)
	{
		using ReceiverValueType = typename std::decay_t<decltype(*WeakReceiver.Pin())>::ValueType;
		
		while (true)
		{
			auto FailableResult = co_await ValueStream;
			
			if (!WeakReceiver.IsValid() || WeakReceiver.Pin()->IsClosed())
			{
				break;
			}
			
			if (FailableResult.template ContainsAnyOf<UEndOfStreamError>())
			{
				WeakReceiver.Pin()->Close();
				break;
			}

			SharedTuple->template Get<Index>().Emplace(MoveTemp(FailableResult));

			const bool bContainsUnset = SharedTuple->ApplyAfter([&](const auto&... OptionalFailableResults)
			{
				return (!OptionalFailableResults.IsSet() || ...);
			});

			if (bContainsUnset)
			{
				continue;
			}

			TArray<UFailableResultError*> Errors;
			SharedTuple->ApplyAfter([&](const auto&... OptionalFailableResults)
			{
				(Errors.Append(OptionalFailableResults->GetErrors()), ...);
			});

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

	template <typename... ValueTypes>
	TValueStream<TTuple<ValueTypes...>> Combine(TValueStream<ValueTypes>&&... ValueStreams)
	{
		TValueStream<TTuple<ValueTypes...>> Ret;
		
		auto SharedTuple = MakeShared<TTuple<TOptional<TFailableResult<ValueTypes>>...>>();
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>)
		{
			(CombineImpl<Indices>(MoveTemp(ValueStreams), Ret.GetReceiver(), SharedTuple), ...);
		}(std::index_sequence_for<ValueTypes...>{});
		
		return Ret;
	}
}
