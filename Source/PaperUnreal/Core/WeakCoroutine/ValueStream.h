// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CancellableFuture.h"
#include "WeakCoroutine.h"
#include "PaperUnreal/Core/Utils.h"


enum class EValueStreamError
{
	StreamEnded,
};


template <typename T, typename... ErrorTypes>
class TValueStreamValueReceiver
{
public:
	TValueStreamValueReceiver() = default;

	TValueStreamValueReceiver(const TValueStreamValueReceiver&) = delete;
	TValueStreamValueReceiver& operator=(const TValueStreamValueReceiver&) = delete;

	TValueStreamValueReceiver(TValueStreamValueReceiver&&) = default;
	TValueStreamValueReceiver& operator=(TValueStreamValueReceiver&&) = default;

	TCancellableFuture<T, EValueStreamError, ErrorTypes...> NextValue()
	{
		if (bEnded)
		{
			return EValueStreamError::StreamEnded;
		}

		if (UnclaimedFutures.Num() > 0)
		{
			return PopFront(UnclaimedFutures);
		}

		auto [Promise, Future] = MakePromise<T, EValueStreamError, ErrorTypes...>();
		Promises.Add(MoveTemp(Promise));
		return MoveTemp(Future);
	}

	template <typename U>
	void ReceiveValue(U&& Value)
	{
		check(!bEnded);

		if (Promises.Num() == 0)
		{
			UnclaimedFutures.Add(MakeReadyFuture<T, EValueStreamError, ErrorTypes...>(Forward<U>(Value)));
			return;
		}

		PopFront(Promises).SetValue(Forward<U>(Value));
	}

	void End()
	{
		bEnded = true;
		UnclaimedFutures.Empty();

		while (Promises.Num() > 0)
		{
			PopFront(Promises).SetValue(EValueStreamError::StreamEnded);
		}
	}

private:
	bool bEnded = false;
	TArray<TCancellableFuture<T, EValueStreamError, ErrorTypes...>> UnclaimedFutures;
	TArray<TCancellablePromise<T, EValueStreamError, ErrorTypes...>> Promises;
};


template <typename T, typename... ErrorTypes>
class TValueStream
{
public:
	using ReceiverType = TValueStreamValueReceiver<T, ErrorTypes...>;

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

	TCancellableFuture<T, EValueStreamError, ErrorTypes...> Next()
	{
		return Receiver->NextValue();
	}

	friend auto operator co_await(TValueStream& Stream) { return Stream.Next(); }

private:
	TSharedPtr<ReceiverType> Receiver = MakeShared<ReceiverType>();
};


bool AllValid(const auto&... Check);


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
			Each.Pin()->ReceiveValue(NewValue);
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
TValueStream<T> CreateMulticastValueStream(const TArray<T>& ReadyValues, DelegateType& MulticastDelegate)
{
	return CreateMulticastValueStream(ReadyValues, MulticastDelegate, []<typename ArgType>(ArgType&& Arg) -> decltype(auto)
	{
		return Forward<ArgType>(Arg);
	});
}


template <typename T, typename DelegateType, typename TransformFuncType>
TValueStream<T> CreateMulticastValueStream(const TArray<T>& ReadyValues, DelegateType& MulticastDelegate, TransformFuncType&& TransformFunc)
{
	TValueStream<T> Ret;

	auto Receiver = Ret.GetReceiver();
	for (const T& Each : ReadyValues)
	{
		Receiver.Pin()->ReceiveValue(Each);
	}

	MulticastDelegate.Add(DelegateType::FDelegate::CreateSPLambda(Receiver.Pin().ToSharedRef(),
		[Receiver, TransformFunc = Forward<TransformFuncType>(TransformFunc)]<typename... ArgTypes>(ArgTypes&&... NewValues)
		{
			Receiver.Pin()->ReceiveValue(TransformFunc(Forward<ArgTypes>(NewValues)...));
		}));

	return Ret;
}


template <typename T, typename PredicateType>
TCancellableFuture<T, EValueStreamError> FirstInStream(TValueStream<T>&& Stream, PredicateType&& Predicate)
{
	return RunWeakCoroutineNoCaptures([](
		TWeakCoroutineContext<T, EValueStreamError>& Context,
		TValueStream<T> Stream,
		std::decay_t<PredicateType> Predicate
	) -> TWeakCoroutine<T, EValueStreamError>
	{
		while (true)
		{
			TVariant<T, EDefaultFutureError, EValueStreamError> Value = co_await WithError(Stream.Next());
	
			if (auto* Error = Value.template TryGet<EDefaultFutureError>())
			{
				continue;
			}
	
			if (auto* Error = Value.template TryGet<EValueStreamError>())
			{
				co_return *Error;
			}
	
			if (Invoke(Predicate, Value.template Get<T>()))
			{
				co_return Value.template Get<T>();
			}
		}
	}, MoveTemp(Stream), Forward<PredicateType>(Predicate)).ReturnValue();
}
