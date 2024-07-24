// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "WeakCoroutine.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "ValueStream.generated.h"


UCLASS()
class UValueStreamError : public UFailableResultError
{
	GENERATED_BODY()

public:
	static UValueStreamError* StreamClosed()
	{
		return NewError<UValueStreamError>(TEXT("StreamClosed"));
	}
};


template <typename T>
class TValueStreamValueReceiver
{
public:
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
		
		if (bEnded)
		{
			return UValueStreamError::StreamClosed();
		}

		auto [Promise, Future] = MakePromise<T>();
		Promises.Add(MoveTemp(Promise));
		return MoveTemp(Future);
	}

	template <typename U>
	void ReceiveValue(U&& Value)
	{
		check(!bEnded);

		if (Promises.Num() == 0)
		{
			UnclaimedFutures.Add(TCancellableFuture<T>{Forward<U>(Value)});
			return;
		}

		PopFront(Promises).SetValue(Forward<U>(Value));
	}

	void End()
	{
		bEnded = true;

		while (Promises.Num() > 0)
		{
			PopFront(Promises).SetValue(UValueStreamError::StreamClosed());
		}
	}

private:
	bool bEnded = false;
	TArray<TCancellableFuture<T>> UnclaimedFutures;
	TArray<TCancellablePromise<T>> Promises;
};


template <typename T>
class TValueStream
{
public:
	using ReceiverType = TValueStreamValueReceiver<T>;

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

	friend TCancellableFutureAwaitable<T>
	operator co_await(TValueStream& Stream) { return operator co_await(Stream.Receiver->NextValue()); }

private:
	TSharedPtr<ReceiverType> Receiver = MakeShared<ReceiverType>();
};


bool AllValid(const auto&... Check);


// TODO dangling pointer for uobject types
template <typename T>
class TValueStreamer
{
public:
	using ValueType = T;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValueReceived, const T&);

	TValueStreamer() = default;
	TValueStreamer(const TValueStreamer&) = delete;
	TValueStreamer& operator=(const TValueStreamer&) = delete;

	TValueStream<T> CreateStream(bool bPutHistoryInStream = true) const
	{
		TValueStream<T> Ret;
		Receivers.Add(Ret.GetReceiver());

		if (bPutHistoryInStream)
		{
			for (const auto& Each : History)
			{
				Receivers.Last().Pin()->ReceiveValue(Each);
			}
		}

		return Ret;
	}

	const TArray<T>& GetHistory() const
	{
		return History;
	}

	template <typename FuncType>
	void Observe(UObject* Object, FuncType&& Func) const
	{
		for (const T& Each : History)
		{
			Func(Each);
		}

		OnValueReceivedDelegate.AddWeakLambda(Object, Forward<FuncType>(Func));
	}

	// TODO rename
	template <typename FuncType>
	void OnStreamEnd(UObject* Object, FuncType&& Func) const
	{
		OnStreamEndDelegate.AddWeakLambda(Object, Forward<FuncType>(Func));
	}

	void ReceiveValue(const T& NewValue)
	{
		if constexpr (std::is_pointer_v<T>)
		{
			if (!AllValid(NewValue))
			{
				return;
			}
		}

		History.Add(NewValue);
		OnValueReceivedDelegate.Broadcast(NewValue);

		for (int32 i = Receivers.Num() - 1; i >= 0; --i)
		{
			if (!Receivers[i].IsValid())
			{
				Receivers.RemoveAt(i);
			}
		}

		auto ReceiversCopy = Receivers;
		for (auto& Each : ReceiversCopy)
		{
			if (auto Pinned = Each.Pin())
			{
				Pinned->ReceiveValue(NewValue);
			}
		}
	}

	void ReceiveValues(const TArray<T>& NewValues)
	{
		for (const T& Each : NewValues)
		{
			ReceiveValue(Each);
		}
	}

	template <typename U> requires std::is_convertible_v<U, T>
	void ReceiveValueIfNotInHistory(U&& NewValue)
	{
		if (!History.Contains(NewValue))
		{
			ReceiveValue(Forward<U>(NewValue));
		}
	}

	void ReceiveValuesIfNotInHistory(const TArray<T>& NewValues)
	{
		for (const T& Each : NewValues)
		{
			ReceiveValueIfNotInHistory(Each);
		}
	}

	void ClearHistory()
	{
		History.Empty();
	}

	void EndStreams(bool bClearHistory = true)
	{
		if (bClearHistory)
		{
			ClearHistory();
		}

		auto ReceiversCopy = MoveTemp(Receivers);
		for (const auto& Each : ReceiversCopy)
		{
			if (auto Pinned = Each.Pin())
			{
				Pinned->End();
			}
		}

		OnStreamEndDelegate.Broadcast();
	}

private:
	TArray<T> History;
	mutable TArray<TWeakPtr<TValueStreamValueReceiver<T>>> Receivers;
	mutable FOnValueReceived OnValueReceivedDelegate;
	mutable FSimpleMulticastDelegate OnStreamEndDelegate;
};


template <typename T, typename DelegateType>
TTuple<TValueStream<T>, FDelegateHandle> CreateMulticastValueStream(
	const TArray<T>& ReadyValues,
	DelegateType& MulticastDelegate)
{
	return CreateMulticastValueStream(ReadyValues, MulticastDelegate,
		[](auto...) { return true; },
		[]<typename ArgType>(ArgType&& Arg) -> decltype(auto) { return Forward<ArgType>(Arg); });
}


template <typename T, typename DelegateType, typename PredicateType, typename TransformFuncType>
TTuple<TValueStream<T>, FDelegateHandle> CreateMulticastValueStream(
	const TArray<T>& ReadyValues,
	DelegateType& MulticastDelegate,
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
		
		FCopyChecker(TWeakPtr<TValueStreamValueReceiver<T>> InReceiver) : Receiver(InReceiver) {}
		FCopyChecker(const FCopyChecker&) { AssertNoCopy(); }
		FCopyChecker& operator=(const FCopyChecker&) { AssertNoCopy(); return *this; }
		FCopyChecker(FCopyChecker&&) = default;
		FCopyChecker& operator=(FCopyChecker&&) = default;

		~FCopyChecker()
		{
			if (auto Pinned = Receiver.Pin())
			{
				Pinned->End();
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
			CopyChecker = FCopyChecker{ Receiver }
		]<typename... ArgTypes>(ArgTypes&&... NewValues)
		{
			if (Predicate(Forward<ArgTypes>(NewValues)...))
			{
				Receiver.Pin()->ReceiveValue(TransformFunc(Forward<ArgTypes>(NewValues)...));
			}
		}));

	return MakeTuple(MoveTemp(Ret), Handle);
}