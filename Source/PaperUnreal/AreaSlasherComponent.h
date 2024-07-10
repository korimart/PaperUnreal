// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerMeshComponent.h"
#include "TracerToAreaConverterComponent.h"
#include "Core/ActorComponent2.h"
#include "Core/Utils.h"
#include "AreaSlasherComponent.generated.h"


UCLASS()
class UAreaSlasherComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetSlashTarget(UAreaBoundaryComponent* Target)
	{
		SlashTarget = Target;
	}

	void SetTracerToAreaConverter(UTracerToAreaConverterComponent* Converter)
	{
		TracerToAreaConverter = Converter;
	}

private:
	UPROPERTY()
	UTracerPathComponent* Slasher;

	UPROPERTY()
	UAreaBoundaryComponent* SlashTarget;

	UPROPERTY()
	UTracerToAreaConverterComponent* TracerToAreaConverter;

	TArray<FWeakAwaitableHandleInt32> FirstInsideIndexAwaitables;
	TArray<FWeakAwaitableHandleInt32> LastInsideIndexAwaitables;
	TArray<TWeakAwaitableHandle<bool>> ConversionAwaitables;

	UAreaSlasherComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(SlashTarget, TracerToAreaConverter));
		Slasher = TracerToAreaConverter->GetTracer();
		check(AllValid(Slasher));

		// TODO destroy self on slash target end play

		// Slasher->OnPathEvent.AddWeakLambda(this, [this](const FTracerPathEvent& Event)
		// {
		// 	if (Event.NewPointGenerated() || Event.LastPointModified())
		// 	{
		// 		DoCollisionTest();
		// 	}
		// });

		TracerToAreaConverter->OnTracerToAreaConversion.AddWeakLambda(
			this, [this](FSegmentArray2D CorrectlyAlignedPath)
			{
				CorrectlyAlignedPath.ReverseVertexOrder();
				SlashTarget->ReduceByPath(CorrectlyAlignedPath);
			});
	}

	FWeakAwaitableInt32 WaitForFirstInsidePointIndex()
	{
		FWeakAwaitableInt32 Ret;
		FirstInsideIndexAwaitables.Add(Ret.GetHandle());
		return Ret;
	}

	FWeakAwaitableInt32 WaitForLastInsidePointIndex()
	{
		FWeakAwaitableInt32 Ret;
		LastInsideIndexAwaitables.Add(Ret.GetHandle());
		return Ret;
	}

	TWeakAwaitable<bool> WaitForTracerToAreaConversion()
	{
		TWeakAwaitable<bool> Ret;
		ConversionAwaitables.Add(Ret.GetHandle());
		return Ret;
	}

	void DoCollisionTest()
	{
		if (Slasher->GetPath().PointCount() == 0)
		{
			return;
		}

		const int32 PointIndex = Slasher->GetPath().PointCount() - 1;
		const TOptional<int32> PrevPointIndex = PointIndex == 0 ? TOptional<int32>{} : PointIndex - 1;
		const TOptional<FVector2D> PrevPoint
			= PrevPointIndex ? Slasher->GetPath().GetPoints()[*PrevPointIndex] : TOptional<FVector2D>{};
		const bool bWasInsideSlashTarget = PrevPoint ? SlashTarget->IsInside(FVector{*PrevPoint, 0.f}) : false;
		const bool bIsInsideSlashTarget = SlashTarget->IsInside(FVector{Slasher->GetPath().GetLastPoint(), 0.f});

		if (!bWasInsideSlashTarget && bIsInsideSlashTarget)
		{
			FlushAwaitablesArray(FirstInsideIndexAwaitables, PointIndex);
		}

		if (bWasInsideSlashTarget && !bIsInsideSlashTarget)
		{
			FlushAwaitablesArray(LastInsideIndexAwaitables, *PrevPointIndex);
		}
	}

	void StartSlashingProcess()
	{
		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			const int32 FirstInsidePointIndex = co_await WaitForFirstInsidePointIndex();
			const int32 LastInsidePointIndex = co_await WaitForLastInsidePointIndex();

			StartSlashingProcess();

			const bool bExpandedToTheLeftOfPath = co_await WaitForTracerToAreaConversion();

			const int32 FirstSegmentIndex = FirstInsidePointIndex == 0 ? 0 : FirstInsidePointIndex - 1;
			const int32 LastSegmentIndex = FMath::Max(FirstSegmentIndex,
				Slasher->GetPath().GetPoints().IsValidIndex(LastInsidePointIndex + 1)
				? LastInsidePointIndex
				: LastInsidePointIndex - 1);

			FSegmentArray2D MinimalSlicingPath = Slasher->GetPath().SubArray(FirstSegmentIndex, LastSegmentIndex);
			MinimalSlicingPath.SetSegment(0, SlashTarget->Clip(MinimalSlicingPath[0]));
			MinimalSlicingPath.SetSegment(-1, SlashTarget->Clip(MinimalSlicingPath[-1]));
			// SlashTarget->ReduceByPath(MoveTemp(MinimalSlicingPath), bExpandedToTheLeftOfPath);
		});
	}
};
