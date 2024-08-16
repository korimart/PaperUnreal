// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LogBattleGameMode.h"
#include "PaperUnreal/AreaTracer/SegmentArray.h"
#include "PaperUnreal/AreaTracer/TracerOverlapCheckerComponent.h"
#include "PaperUnreal/AreaTracer/TracerPathComponent.h"
#include "PaperUnreal/GameFramework2/ComponentGroupComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/KillZComponent.h"
#include "PaperUnreal/GameMode/ModeAgnostic/LifeComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "BattlePawnKillerComponent.generated.h"


UCLASS()
class UBattlePawnKillerComponent : public UComponentGroupComponent
{
	GENERATED_BODY()

public:
	void SetTracer(UTracerPathComponent* InTracer)
	{
		check(!HasBeenInitialized());
		Tracer = InTracer;
	}

	void SetArea(UAreaBoundaryComponent* InArea)
	{
		check(!HasBeenInitialized());
		Area = InArea;
	}

	void SetOverlapChecker(UTracerOverlapCheckerComponent* InOverlapChecker)
	{
		check(!HasBeenInitialized());
		OverlapChecker = InOverlapChecker;
	}

private:
	UPROPERTY()
	UTracerPathComponent* Tracer;

	UPROPERTY()
	UAreaBoundaryComponent* Area;

	UPROPERTY()
	UTracerOverlapCheckerComponent* OverlapChecker;

	UBattlePawnKillerComponent()
	{
		bWantsInitializeComponent = true;
		SetIsReplicatedByDefault(false);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(Tracer);
		AddLifeDependency(Area);
		AddLifeDependency(OverlapChecker);

		Area->GetBoundary().Observe(this, [this](const FLoopedSegmentArray2D& Boundary)
		{
			const FSegmentArray2D& RunningTracerPath = Tracer->GetRunningPath();
			
			if (RunningTracerPath.IsValid())
			{
				// 트레이서의 첫 포인트가 영역 경계 위에 있기 때문에 안전한 체크를 위해서 영역 안으로 살짝 넣음
				const FSegment2D FirstSegment = RunningTracerPath[0];
				const FVector2D TracerOrigin = FirstSegment.StartPoint() - FirstSegment.Direction * UE_KINDA_SMALL_NUMBER;

				if (!Boundary.IsInside(TracerOrigin))
				{
					UE_LOG(LogBattleGameMode, Log, TEXT("%p 내 트레이서의 시작점이 위치하던 영역이 없어져 사망합니다."), this);
					KillPlayer(GetOwner());
				}
			}

			if (!RunningTracerPath.IsValid() && !Boundary.IsInside(FVector2D{GetOwner()->GetActorLocation()}))
			{
				UE_LOG(LogBattleGameMode, Log, TEXT("%p 내가 서 있던 영역이 없어져 사망합니다"), this);
				KillPlayer(GetOwner());
			}
		});

		OverlapChecker->OnTracerBumpedInto.AddWeakLambda(this, [this](UTracerPathComponent* Bumpee)
		{
			UE_LOG(LogBattleGameMode, Log, TEXT("%p -> %p 트레이서 충돌로 인한 킬"), this, Bumpee);
			KillPlayer(Bumpee->GetOwner());
		});

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
		{
			auto KillZComponent = NewChildComponent<UKillZComponent>();
			KillZComponent->RegisterComponent();
			co_await KillZComponent->OnKillZ;
			
			UE_LOG(LogBattleGameMode, Log, TEXT("%p KillZ로 인해 사망합니다"), this);
			KillPlayer(GetOwner());
		});
	}

	static void KillPlayer(AActor* Player)
	{
		if (IsValid(Player))
		{
			if (ULifeComponent* Life = Player->FindComponentByClass<ULifeComponent>())
			{
				Life->SetbAlive(false);
			}
		}
	}
};
