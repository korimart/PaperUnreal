// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/UECoroutine.h"
#include "Net/UnrealNetwork.h"
#include "ByteArrayReplicatorComponent.generated.h"


template <typename T>
concept CByteArray = std::is_convertible_v<T, TArray<uint8>>;


enum class EArrayEvent
{
	Reset,
	Appended,
	LastModified,
};


template <typename T>
struct TArrayEvent
{
	EArrayEvent Event;
	TArray<T> Affected;

	bool IsReset() const { return Event == EArrayEvent::Reset; }
	bool IsAppended() const { return Event == EArrayEvent::Appended; }
	bool IsLastModified() const { return Event == EArrayEvent::LastModified; }
};


using FByteArrayEvent = TArrayEvent<uint8>;


USTRUCT()
struct FRepByteArray
{
	GENERATED_BODY()

	const TArray<uint8>& GetArray() const
	{
		return Array;
	}

	template <CByteArray ArrayType>
	void Reset(ArrayType&& NewArray)
	{
		Id++;
		Array = Forward<ArrayType>(NewArray);
	}

	template <CByteArray ArrayType>
	void Append(ArrayType&& NewArray)
	{
		Array.Append(Forward<ArrayType>(NewArray));
	}

	template <CByteArray ArrayType>
	void SetLast(ArrayType&& NewArray)
	{
		check(Array.Num() >= NewArray.Num());

		if (NewArray.Num() > 0)
		{
			Array.SetNum(Array.Num() - NewArray.Num(), false);
			Append(Forward<ArrayType>(NewArray));
		}
	}

	TArray<FByteArrayEvent> CompareAndCreateEvents(const FRepByteArray& OldArray) const
	{
		if (Id != OldArray.Id)
		{
			return {{.Event = EArrayEvent::Reset, .Affected = Array,}};
		}

		// 현재 원소 삭제 연산은 지원하지 않는데 추가해놓고 이 함수를 수정하는 것을 까먹은 경우
		check(OldArray.Array.Num() <= Array.Num());

		const int32 ModifiedByteCount = [&]()
		{
			for (int32 i = 0; i < OldArray.Array.Num(); i++)
			{
				if (OldArray.Array[i] != Array[i])
				{
					return OldArray.Array.Num() - i;
				}
			}
			return 0;
		}();

		const int32 AppendedByteCount = Array.Num() - OldArray.Array.Num();

		TArray<FByteArrayEvent> Ret;

		if (ModifiedByteCount > 0)
		{
			Ret.Add({
				.Event = EArrayEvent::LastModified,
				.Affected = TArray<uint8>{TArrayView<const uint8>{OldArray.Array}.Right(ModifiedByteCount)}
			});
		}

		if (AppendedByteCount > 0)
		{
			Ret.Add({
				.Event = EArrayEvent::Appended,
				.Affected = TArray<uint8>{TArrayView<const uint8>{Array}.Right(AppendedByteCount)}
			});
		}

		return Ret;
	}

private:
	UPROPERTY()
	int32 Id = -1;

	UPROPERTY()
	TArray<uint8> Array;
};


UCLASS()
class UByteArrayReplicatorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnByteArrayEvent, const FByteArrayEvent&);
	FOnByteArrayEvent OnByteArrayEvent;

	TValueGenerator<FByteArrayEvent> CreateEventGenerator()
	{
		return CreateMulticastValueGenerator(RepArray.CompareAndCreateEvents({}), OnByteArrayEvent);
	}

	const TArray<uint8>& GetArray() const
	{
		return RepArray.GetArray();
	}

	template <CByteArray ArrayType>
	void ResetArray(ArrayType&& NewArray)
	{
		check(GetNetMode() != NM_Client);
		RepArray.Reset(Forward<ArrayType>(NewArray));
	}

	template <CByteArray ArrayType>
	void AppendArray(ArrayType&& NewArray)
	{
		check(GetNetMode() != NM_Client);
		RepArray.Append(Forward<ArrayType>(NewArray));
	}

	void SetLast(const TArray<uint8>& NewData)
	{
		check(GetNetMode() != NM_Client);
		RepArray.SetLast(NewData);
	}

	template <typename EventType> requires std::is_convertible_v<EventType, FByteArrayEvent>
	void SetFromEvent(EventType&& Event)
	{
		if (Event.Event == EArrayEvent::Reset)
		{
			ResetArray(Forward<EventType>(Event).Affected);
		}
		else if (Event.Event == EArrayEvent::Appended)
		{
			AppendArray(Forward<EventType>(Event).Affected);
		}
		else if (Event.Event == EArrayEvent::LastModified)
		{
			SetLast(Forward<EventType>(Event).Affected);
		}
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_Array)
	FRepByteArray RepArray;

	UByteArrayReplicatorComponent()
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
	void OnRep_Array(const FRepByteArray& OldArray)
	{
		for (const FByteArrayEvent& Each : RepArray.CompareAndCreateEvents(OldArray))
		{
			OnByteArrayEvent.Broadcast(Each);
		}
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepArray);
	}
};


template <typename T, typename U>
concept CTypedByteArray = std::is_convertible_v<T, TArray<U>>;


template <typename T>
class TTypedByteArrayReplicatorView : public TSharedFromThis<TTypedByteArrayReplicatorView<T>>
{
public:
	TTypedByteArrayReplicatorView(UByteArrayReplicatorComponent& Replicator)
		: ByteArrayReplicator(Replicator)
	{
	}

	TValueGenerator<TArrayEvent<T>> CreateTypedEventGenerator()
	{
		TValueGenerator<TArrayEvent<T>> Ret;
		RunWeakCoroutine(this->AsShared(), [this, Receiver = Ret.GetReceiver()](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			Context.AddToWeakList(Receiver);
			for (auto ByteEvents = ByteArrayReplicator.CreateEventGenerator();;)
			{
				Receiver.Pin()->AddValue(ToTypedEvent(co_await ByteEvents.Next()));
			}
		});
		return Ret;
	}
	
	void ResetArray(const TArray<T>& NewArray)
	{
		ByteArrayReplicator.ResetArray(Serialize(NewArray));
	}

	void AppendArray(const TArray<T>& NewArray)
	{
		ByteArrayReplicator.AppendArray(Serialize(NewArray));
	}

	void SetLast(const TArray<T>& NewArray)
	{
		ByteArrayReplicator.SetLast(Serialize(NewArray));
	}

	void SetFromEvent(const TArrayEvent<T>& Event)
	{
		ByteArrayReplicator.SetFromEvent(FByteArrayEvent{
			.Event = Event.Event,
			.Affected = Serialize(Event.Affected),
		});
	}

	TArrayEvent<T> ToTypedEvent(const FByteArrayEvent& Event)
	{
		return { .Event = Event.Event, .Affected = Deserialize(Event.Affected), };
	}

private:
	UByteArrayReplicatorComponent& ByteArrayReplicator;

	TArray<uint8> Serialize(const TArray<T>& Source)
	{
		TArray<uint8> Serialized;
		FMemoryWriter Writer{Serialized};
		for (const T& Each : Source)
		{
			Writer << const_cast<T&>(Each);
		}
		return Serialized;
	}
	
	TArray<T> Deserialize(const TArray<uint8>& Source)
	{
		FMemoryReader Reader{Source};
		TArray<T> Ret;
		while (!Reader.AtEnd())
		{
			T NewElement;
			Reader << NewElement;
			Ret.Add(NewElement);
		}
		return Ret;
	}
};
