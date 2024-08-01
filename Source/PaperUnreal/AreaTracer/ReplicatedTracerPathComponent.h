// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "TracerPathProvider.h"
#include "Net/UnrealNetwork.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "ReplicatedTracerPathComponent.generated.h"


/**
 * 언리얼 Replication에 대해 코드 분석한 내용을 이곳에 남김
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


USTRUCT()
struct FOptionalVector2D
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSet;

	UPROPERTY()
	FVector2D Vector2D;

	FOptionalVector2D& operator=(const TOptional<FVector2D>& Other)
	{
		if (Other)
		{
			bSet = true;
			Vector2D = *Other;
		}
		else
		{
			bSet = false;
			Vector2D = {};
		}

		return *this;
	}
};


USTRUCT()
struct FNumberedVector2DArray
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Number = 0;

	UPROPERTY()
	TArray<FVector2D> PathTail;
};


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponent2, public ITracerPathProvider
{
	GENERATED_BODY()

public:
	virtual TLiveDataView<TOptional<FVector2D>> GetRunningPathHead() const override { return PathHead; }
	virtual TLiveDataView<TArray<FVector2D>> GetRunningPathTail() const override { return PathTail; }

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		check(!HasBeenInitialized());
		ServerTracerPath = Source;
	}

private:
	UPROPERTY(ReplicatedUsing=OnRep_PathHead)
	FOptionalVector2D RepPathHead;
	mutable TLiveData<TOptional<FVector2D>> PathHead;

	UPROPERTY(ReplicatedUsing=OnRep_PathTail)
	FNumberedVector2DArray RepPathTail;
	mutable TLiveData<TArray<FVector2D>> PathTail;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
		DOREPLIFETIME(ThisClass, RepPathHead);
		DOREPLIFETIME(ThisClass, RepPathTail);
	}

	UFUNCTION()
	void OnRep_PathHead()
	{
		PathHead = RepPathHead.bSet ? TOptional{RepPathHead.Vector2D} : TOptional<FVector2D>{};
	}

	UFUNCTION()
	void OnRep_PathTail(const FNumberedVector2DArray& Old)
	{
		if (RepPathTail.Number == Old.Number)
		{
			for (int32 i = Old.PathTail.Num(); i < RepPathTail.PathTail.Num(); i++)
			{
				PathTail.Add(RepPathTail.PathTail[i]);
			}
		}
		else
		{
			PathTail.SetValueSilent(RepPathTail.PathTail);
			PathTail.NotifyDiff(Old.PathTail);
		}
	}

	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	UReplicatedTracerPathComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(true);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		if (GetNetMode() != NM_Client)
		{
			AddLifeDependency(ServerTracerPath);
			InitiateTracerPathRelay();
		}
	}

	void InitiateTracerPathRelay()
	{
		ServerTracerPath->GetRunningPathHead().Observe(this, [this](const TOptional<FVector2D>& Head)
		{
			RepPathHead = Head;
		});

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			while (true)
			{
				auto Stream = ServerTracerPath->GetRunningPathTail().CreateStrictAddStream();
				while (auto NextPoint = co_await WithError<UEndOfStreamError>(Stream))
				{
					RepPathTail.PathTail.Add(NextPoint.GetResult());
				}
				RepPathTail.Number++;
				RepPathTail.PathTail.Empty();
			}
		});
	}
};
