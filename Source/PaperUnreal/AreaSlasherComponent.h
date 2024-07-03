// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerCollisionStateComponent.h"
#include "TracerMeshComponent.h"
#include "TracerToAreaConverterComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/Utils.h"
#include "AreaSlasherComponent.generated.h"


UCLASS()
class UAreaSlasherComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void SetCollisionState(UTracerCollisionStateComponent* State)
	{
		CollisionState = State;
	}

	void SetSlashTarget(UAreaMeshComponent* Target)
	{
		SlashTarget = Target;
	}

	void SetTracerToAreaConverter(UTracerToAreaConverterComponent* Converter)
	{
		TracerToAreaConverter = Converter;
	}

private:
	UPROPERTY()
	UTracerCollisionStateComponent* CollisionState;

	UPROPERTY()
	UAreaMeshComponent* SlashTarget;

	UPROPERTY()
	UTracerToAreaConverterComponent* TracerToAreaConverter;

	struct FEntryExitPoint
	{
		FVector EntryPoint;
		FVector ExitPoint;
	};

	struct FEntryExitPoints
	{
		void OnEntry(const FVector& EntryPoint)
		{
			PendingEntryPoint = EntryPoint;
		}

		void OnExit(const FVector& ExitPoint)
		{
			if (PendingEntryPoint)
			{
				EntryExitPoints.Add({
					.EntryPoint = *std::exchange(PendingEntryPoint, {}),
					.ExitPoint = ExitPoint,
				});
			}
		}

		void Empty()
		{
			EntryExitPoints.Empty();
		}

		const TArray<FEntryExitPoint>& Get() const
		{
			return EntryExitPoints;
		}

	private:
		TOptional<FVector> PendingEntryPoint;
		TArray<FEntryExitPoint> EntryExitPoints;
	};

	FEntryExitPoints EntryExitPoints;

	UAreaSlasherComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(CollisionState, SlashTarget, TracerToAreaConverter));

		// TODO destroy self on slash target end play

		CollisionState->FindOrAddCollisionWith(SlashTarget).ObserveValid(this, [this](bool bColliding)
		{
			if (bColliding)
			{
				// EntryExitPoints.OnEntry(CollisionState->GetPlayerLocation());
			}
			else
			{
				// EntryExitPoints.OnExit(CollisionState->GetPlayerLocation());
			}
		});

		TracerToAreaConverter->OnTracerToAreaConversion.AddWeakLambda(this, [this](const FSegmentArray2D& Tracer, bool bToTheLeft)
		{
			// 영역이 맞닿아 있는 경우 SlashTarget에서 나가기 전에 Tracer-to-Area conversion이 발생할 수 있음
			// 이 때 그냥 SlashTarget 에서 나갔다고 가정한다. 어차피 가장 가까운 SlashTarget의 경계 상의 지점을 기준으로
			// 잘라내기 때문에 나가기 전에 하나 후에 하나 똑같음
			// EntryExitPoints.OnExit(CollisionState->GetPlayerLocation());

			for (const auto& [EntryPoint, ExitPoint] : EntryExitPoints.Get())
			{
				const FSegmentArray2D::FIntersection EntrySegment = Tracer.FindClosestPointTo(FVector2D{EntryPoint});
				const FSegmentArray2D::FIntersection ExitSegment = Tracer.FindClosestPointTo(FVector2D{ExitPoint});
				
				FSegmentArray2D MinimalSlicingPath = Tracer.SubArray(EntrySegment.SegmentIndex, ExitSegment.SegmentIndex);
				MinimalSlicingPath.SetPoint(0, EntrySegment.Location(Tracer));
				MinimalSlicingPath.SetLastPoint(ExitSegment.Location(Tracer));
				SlashTarget->ReduceByPath(MoveTemp(MinimalSlicingPath), bToTheLeft);
			}

			EntryExitPoints.Empty();
		});
	}
};
