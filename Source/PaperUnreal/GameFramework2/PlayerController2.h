// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "PaperUnreal/WeakCoroutine/LiveData.h"
#include "PlayerController2.generated.h"

/**
 * 
 */
UCLASS()
class APlayerController2 : public APlayerController
{
	GENERATED_BODY()

public:
	auto GetPossessedPawn() { return ToLiveDataView(PossessedPawn); }
	
protected:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChangedDynamicCallback);
	}

	// 아래는 서버가 지정하는 Pawn이 없을 때 클라가 클라폰(Spectator Pawn)을 사용하게 하기 위한 코드인데
	// 일단 배경지식은 다음과 같음
	//
	// 배경지식 1
	// 1. 기본적으로 플레이어 컨트롤러는 Spectating State로 시작함 그리고 State는 Replicate되는 변수가 아님
	// 2. 서버에서 Pawn이 오면 클라가 Playing State로 바꿈
	// 3. ClientGoToState 사용해서도 서버에서 클라 상태를 변경할 수 있긴 함
	//
	// 배경지식 2
	// 1. ViewTarget은 APlayerCameraManager의 멤버로 AActor이고 AActor::CalcCamera를 호출해서 카메라의 위치가 계산됨
	// 2. 기본구현 보면 카메라 컴포넌트 있으면 카메라 컴포넌트한테 계산 넘김
	// 3. ViewTarget은 기본적으로 PC임
	// 4. APlayerCameraManager는 ViewTarget이 바뀔 대마다 ClientSetViewTarget RPC 호출해서 클라에서도 바꿈
	//
	// 배경지식 3
	// 1. PC의 bAutoManageActiveCameraTarget가 참이면 Pawn이 바뀔 때마다 ViewTarget을 Pawn으로 변경함
	// 2. Pawn이 nullptr이면 자신으로 변경함
	// 3. 근데 이걸 서버에서도 하고 클라에서도 함
	// 3-1. 서버에서 Pawn 설정하면 서버에서 ViewTarget 변경 -> 위에서 언급한 RPC 전송
	// 3-2. 클라에 Pawn이 Replicate 되어 도착하면 Pawn 설정하면서 ViewTarget 변경
	// 4. 즉 RPC랑 replication 중에서 먼저 도착하는 쪽이 ViewTarget을 변경함
	//
	// 이제 게임이 끝났다고 가정하고 서버가 클라한테 Spectating State로 바꾸라고 RPC 날리면
	// 클라는 안 바꿈 왜냐면 클라가 ServerSetSpectatorLocation을 호출하면서 서버랑 클라의 State가 다르면
	// 서버가 클라한테 자신의 상태를 보냄 그래서 Playing으로 다시 바뀜
	// 그래서 서버의 상태를 Spectating State로 바꾸면 자동으로 unpossess되면서 pawn이 nullptr가 됨
	// (근데 서버의 상태를 바꿨다고 해서 클라의 상태가 자동으로 바뀌는 건 아님 그래서 따로 바꿔줘야 됨)
	//
	// 그러면 다음과 같은 일들이 일어남
	// 1. pawn이 nullptr이니까 APlayerCameraManager는 클라한테 ViewTarget PC로 바꾸라고 RPC 보냄
	// 2. nullptr인 pawn이 replicate되면서 ViewTarget을 PC 로 바꿈
	// 3. 클라의 상태를 스펙테이터로 바꾸기 위해 ClientGoToState 호출함 그럼 클라는 Spectator Pawn을 스폰함
	// 여기서 1번과 3번의 순서는 보장할 수 있는데 2번과 3번이 Replication과 RPC이므로 할 수 없음
	// 그래서 2번이 늦게 도착하면 pawn이 nullptr이라서 possession은 바뀌지 않지만 view target을 바꿔버림
	//
	// 가장 간단한 해결방안 -> Spectator Pawn이 있을 땐 항상 Spectator Pawn에게 ViewTarget을 준다

	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams) override
	{
		Super::SetViewTarget(GetSpectatorPawn() ? GetSpectatorPawn() : NewViewTarget, TransitionParams);
	}

private:
	TLiveData<APawn*> PossessedPawn;
	
	UFUNCTION()
	void OnPossessedPawnChangedDynamicCallback(APawn* InOldPawn, APawn* InNewPawn)
	{
		PossessedPawn = InNewPawn;
	}
};
