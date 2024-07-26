// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerOverlapCheckerComponent.h"
#include "TracerPathComponent.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/ModeAgnostic/LifeComponent.h"
#include "TracerKillerComponent.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogTracerKiller, Log, All);


UCLASS()
class UTracerKillerComponent : public UActorComponent2
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

	UTracerKillerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(Tracer);
		AddLifeDependency(Area);
		AddLifeDependency(OverlapChecker);

		Area->GetBoundary().Observe(this, [this](const FLoopedSegmentArray2D& Boundary)
		{
			const FSegmentArray2D& RunningTracerPath = Tracer->GetRunningPathAsSegments();
			
			if (RunningTracerPath.IsValid())
			{
				// 트레이서의 첫 포인트가 영역 경계 위에 있기 때문에 안전한 체크를 위해서 영역 안으로 살짝 넣음
				const FSegment2D FirstSegment = RunningTracerPath[0];
				const FVector2D TracerOrigin = FirstSegment.StartPoint() - FirstSegment.Direction * UE_KINDA_SMALL_NUMBER;

				if (!Boundary.IsInside(TracerOrigin))
				{
					UE_LOG(LogTracerKiller, Log, TEXT("%p 내 트레이서의 시작점이 위치하던 영역이 없어져 사망합니다."), this);
					KillPlayer(GetOwner());
				}
			}

			if (!RunningTracerPath.IsValid() && !Boundary.IsInside(FVector2D{GetOwner()->GetActorLocation()}))
			{
				UE_LOG(LogTracerKiller, Log, TEXT("%p 내가 서 있던 영역이 없어져 사망합니다"), this);
				KillPlayer(GetOwner());
			}
		});

		OverlapChecker->OnTracerBumpedInto.AddWeakLambda(this, [this](UTracerPathComponent* Bumpee)
		{
			UE_LOG(LogTracerKiller, Log, TEXT("%p -> %p 트레이서 충돌로 인한 킬"), this, Bumpee);
			KillPlayer(Bumpee->GetOwner());
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
