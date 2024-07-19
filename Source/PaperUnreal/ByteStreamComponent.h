// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/WeakCoroutine/WeakCoroutine.h"
#include "Core/WeakCoroutine/ValueStream.h"
#include "Net/UnrealNetwork.h"
#include "ByteStreamComponent.generated.h"


template <typename T>
concept CByteArray = std::is_convertible_v<T, TArray<uint8>>;


enum class EStreamEvent
{
	Appended,
	LastModified,
};


template <typename T>
struct TStreamEvent
{
	using ValueType = T;
	
	EStreamEvent Event;
	TArray<T> Affected;

	bool IsAppendedEvent() const { return Event == EStreamEvent::Appended; }
	bool IsLastModifiedEvent() const { return Event == EStreamEvent::LastModified; }

	TStreamEvent<uint8> Serialize() const requires !std::is_same_v<T, uint8>;

	template <typename TargetType>
	TargetType Deserialize() const requires std::is_same_v<T, uint8>;
};

template <typename T>
TStreamEvent<uint8> TStreamEvent<T>::Serialize() const requires !std::is_same_v<T, uint8>
{
	TArray<uint8> Serialized;
	FMemoryWriter Writer{Serialized};

	for (const T& Each : Affected)
	{
		Writer << const_cast<T&>(Each);
	}

	return {
		.Event = Event,
		.Affected = Serialized,
	};
}

template <typename T>
template <typename TargetType>
TargetType TStreamEvent<T>::Deserialize() const requires std::is_same_v<T, uint8>
{
	FMemoryReader Reader{Affected};
	TArray<typename TargetType::ValueType> Deserialized;

	while (!Reader.AtEnd())
	{
		typename TargetType::ValueType NewElement;
		Reader << NewElement;
		Deserialized.Add(NewElement);
	}

	return {
		.Event = Event,
		.Affected = Deserialized,
	};
}


using FByteStreamEvent = TStreamEvent<uint8>;


USTRUCT()
struct FRepByteStream
{
	GENERATED_BODY()

	const TArray<uint8>& GetBytes() const
	{
		return Array;
	}

	bool IsOpen() const
	{
		return bOpen;
	}

	bool IsNewStream(const FRepByteStream& OldStream) const
	{
		return Id != OldStream.Id;
	}

	// 클라이언트에서 Modified Event를 감지해 Affected에 바이트를 넣을 때
	// 몇 바이트 기준으로 넣을 것인지 (즉 1바이트만 감지해도 이게 4면 주변 3바이트를 같이 넣음)
	// TODO ByteStream이 이걸 알아야 되는게 좀 별로 같은데 다른 방법은 없는지 생각해보자
	void SetChunkSize(int32 ByteCount)
	{
		ChunkSize = ByteCount;
	}

	void Open()
	{
		check(!bOpen);
		Id++;
		bOpen = true;
		Array.Empty();
	}

	void Close()
	{
		bOpen = false;
	}

	template <CByteArray ArrayType>
	void PushBytes(ArrayType&& NewArray)
	{
		Array.Append(Forward<ArrayType>(NewArray));
	}

	template <CByteArray ArrayType>
	void ModifyLastBytes(ArrayType&& NewArray)
	{
		check(Array.Num() >= NewArray.Num());

		if (NewArray.Num() > 0)
		{
			Array.SetNum(Array.Num() - NewArray.Num(), false);
			PushBytes(Forward<ArrayType>(NewArray));
		}
	}

	TArray<FByteStreamEvent> CompareAndCreateEvents(const FRepByteStream& OldStream) const
	{
		TArray<FByteStreamEvent> Ret;

		if (Id != OldStream.Id)
		{
			if (Array.Num() > 0)
			{
				Ret.Add({.Event = EStreamEvent::Appended, .Affected = Array,});
			}

			return Ret;
		}

		// 현재 원소 삭제 연산은 지원하지 않는데 추가해놓고 이 함수를 수정하는 것을 까먹은 경우
		check(OldStream.Array.Num() <= Array.Num());

		const int32 ModifiedByteCount = [&]()
		{
			for (int32 i = 0; i < OldStream.Array.Num(); i++)
			{
				if (OldStream.Array[i] != Array[i])
				{
					return (OldStream.Array.Num() - i + ChunkSize - 1) / ChunkSize * ChunkSize;
				}
			}
			return 0;
		}();

		const int32 ModificationStartIndex = OldStream.Array.Num() - ModifiedByteCount;
		const int32 AppendedByteCount = Array.Num() - OldStream.Array.Num();

		if (ModifiedByteCount > 0)
		{
			Ret.Add({
				.Event = EStreamEvent::LastModified,
				.Affected = TArray<uint8>{TArrayView<const uint8>{&Array[ModificationStartIndex], ModifiedByteCount}},
			});
		}

		if (AppendedByteCount > 0)
		{
			Ret.Add({
				.Event = EStreamEvent::Appended,
				.Affected = TArray<uint8>{TArrayView<const uint8>{Array}.Right(AppendedByteCount)}
			});
		}

		return Ret;
	}

private:
	UPROPERTY()
	int32 Id = -1;

	UPROPERTY()
	bool bOpen = false;

	UPROPERTY()
	TArray<uint8> Array;

	int32 ChunkSize = 1;
};


UCLASS()
class UByteStreamComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	const TValueStreamer<FByteStreamEvent>& GetByteStreamer() const
	{
		return ByteStreamer;
	}

	const TArray<uint8>& GetBytes() const
	{
		return RepStream.GetBytes();
	}

	void SetChunkSize(int32 ByteCount)
	{
		RepStream.SetChunkSize(ByteCount);
	}

	void OpenStream()
	{
		check(GetNetMode() != NM_Client);
		RepStream.Open();
	}

	void CloseStream()
	{
		check(GetNetMode() != NM_Client);
		RepStream.Close();
	}

	template <CByteArray ArrayType>
	void PushBytes(ArrayType&& Bytes)
	{
		check(GetNetMode() != NM_Client);
		RepStream.PushBytes(Forward<ArrayType>(Bytes));
	}

	void ModifyLastBytes(const TArray<uint8>& Bytes)
	{
		check(GetNetMode() != NM_Client);
		RepStream.ModifyLastBytes(Bytes);
	}

	template <typename EventType> requires std::is_convertible_v<EventType, FByteStreamEvent>
	void TriggerEvent(EventType&& Event)
	{
		if (Event.Event == EStreamEvent::Appended)
		{
			PushBytes(Forward<EventType>(Event).Affected);
		}
		else if (Event.Event == EStreamEvent::LastModified)
		{
			ModifyLastBytes(Forward<EventType>(Event).Affected);
		}
	}

	template <typename StreamerType>
	void SetInputStream(UE_LIFETIMEBOUND StreamerType& Streamer)
	{
		RunWeakCoroutine(this, [this, &Streamer](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			while (true)
			{
				OpenStream();
				auto PathEvents = Streamer.CreateStream();
				// TODO await
				// while (co_await PathEvents.NextIfNotEnd())
				// {
				// 	TriggerEvent(PathEvents.Pop().Serialize());
				// }
				CloseStream();
			}
		});
	}
	
	template <typename StreamerType>
	void SetOutputStream(UE_LIFETIMEBOUND StreamerType& Streamer)
	{
		RunWeakCoroutine(this, [this, &Streamer](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			auto F = Finally([&](){ Streamer.EndStreams(); });
			while (true)
			{
				auto ByteEvents = GetByteStreamer().CreateStream();
				// TODO await
				// while (co_await ByteEventsNext())
				// {
				// 	Streamer.ReceiveValue(ByteEvents.Pop().Deserialize<typename StreamerType::ValueType>());
				// }
				Streamer.EndStreams();
			}
		});
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_Stream)
	FRepByteStream RepStream;

	TValueStreamer<FByteStreamEvent> ByteStreamer;

	UByteStreamComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	/**
	 * 언리얼 Replication에 대해 코드 분석한 내용을 아래에 남김
	 *
	 * 기본적으로 언리얼의 네트워크 데이터는 Bunch 안에 넣어져서 보내지고 이 Bunch 단위로 필요시 재전송이 이루어짐
	 * Bunch를 여러 패킷으로 쪼갠 Partial Bunch도 존재하는데 이건 밑에서 더 설명
	 *
	 * Bunch는 Reliable과 Unreliable 상태가 존재하는데 상태에 관계 없이 클라이언트가 Ack를 보냈는지 안 보냈는지를 알 수 있음
	 * Unreliable은 fire-and-forget 방식으로 ack 여부를 (알 수는 있지만) 체크 안 함. 보내고 바로 Bunch 파괴.
	 * Reliable은 Ack 받을 때까지 Bunch를 파괴하지 않고 들고 있다가 다시 보냄
	 *
	 * Bunch가 만들어지는 경로는 크게 두 가지로 볼 수 있음
	 * 1. RPC 함수를 호출해서 RPC 함수 argument들을 가지고 Bunch를 만든 경우. 이 때 UFUNCTION의 Flag에 따라 Bunch의 Re/Unreliable이 갈림
	 * 2. Network Frequency마다 액터가 자신의 Property 중에서 값이 변경된 것들을 Bunch로 만듬. 기본적으로 Unreliable bunch
	 *
	 * 2번의 경우 unreliable이기 때문에 Bunch가 전송되고 나면 파괴되어 전송 시점의 값은 소실됨
	 * 하지만 ACK가 왔는지 안 왔는지를 액터가 체크하여 안 왔으면 해당 Property의 더티 상태를 해제하지 않음
	 * 그리고 다음 Network Frequency에서 Bunch를 만들 때 해당 Property들을 같이 Bunch에 담음
	 * 이게 Replicated property의 경우 가장 최신의 값만 클라에 전송되는 원리임. Bunch는 unreliable이지만 액터가 reliable를 보장하는 형태
	 * 즉 클라가 ACK를 보내지 않으면 network frequency마다 계속해서 패킷을 보내게 된다.
	 *
	 * 기본적으로 한 Bunch 안에 담을 수만 있다면 액터 내 서로 다른 Property들이 동시에 클라이언트에 도착함을 보장할 수 있을 것 같다.
	 *
	 * Partial Bunch는 뭐냐면 Bunch가 너무 크면 보내기 전에 쪼개서 보냄. 근데 받은 다음 Partial Bunch를 다시 합쳐서 Bunch로 만드므로 구현 디테일이긴 함
	 * 1. Reliable bunch의 경우 partial bunch도 순서대로 도착함이 보장되기 때문에 bunch로 잘 합쳐짐
	 * 2. Unreliable bunch의 경우 partial bunch가 순서대로 도착하지 않았으면 해당 bunch 전체가 도착하지 않았다고 처리해버림
	 *
	 * Replicated Property 입장에서 보면 2번은 그냥 bunch가 도착을 안 한 것과 같으므로
	 * 서버는 partial bunch들이 우연히 순서대로 도착할 때까지 계속해서 해당 bunch를 보내게 된다.
	 * (Packet loss를 높이고 로그를 보면 확인할 수 있음)
	 *
	 * 여기서 조심할 점이 있는데 Partial Bunch의 개수가 너무 많으면 (GCVarNetPartialBunchReliableThreshold를 초과하면)
	 * unreliable bunch들도 reliable로 전환되고, 이 partial bunch들의 전송이 완료될 때까지 해당 actor channel을 통한 모든 전송이 막힘
	 * 자세한 건 저 console var 검색해서 코드 읽어볼 것
	 *
	 * 코드 분석 및 실험을 통해서 위 사실을 확인했지만 틀린 내용이 있을 수 있으므로 주의해야 함. 일단 gpt-4o는 내 말이 맞다고 해줬음
	 * @see LogNetPartialBunch VeryVerbose
	 * @see UChannel::SendBunch
	 * @see UChannel::ReceivedBunch
	 * (OnRep 함수에 브레이크 포인트 찍어서 콜스택 보면 관련 함수들을 많이 볼 수 있음)
	 */
	UFUNCTION()
	void OnRep_Stream(const FRepByteStream& OldStream)
	{
		if (RepStream.IsNewStream(OldStream))
		{
			ByteStreamer.EndStreams();
		}
		
		ByteStreamer.ReceiveValues(RepStream.CompareAndCreateEvents(OldStream));

		if (!RepStream.IsOpen())
		{
			ByteStreamer.EndStreams();
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepStream);
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		Super::EndPlay(EndPlayReason);
		
		ByteStreamer.EndStreams();
	}
};
