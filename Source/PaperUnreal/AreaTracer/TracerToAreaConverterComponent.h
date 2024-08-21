// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "TracerToAreaConverterComponent.generated.h"


UCLASS()
class UTracerToAreaConverterComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTracerToAreaConversion, const FSegmentArray2D&);
	FOnTracerToAreaConversion OnTracerToAreaConversion;

	void SetTracer(UTracerPathComponent* InTracer)
	{
		check(!HasBeenInitialized())
		Tracer = InTracer;
	}

	void SetConversionDestination(UAreaBoundaryComponent* Destination)
	{
		check(!HasBeenInitialized())
		ConversionDestination = Destination;
	}

	UTracerPathComponent* GetTracer() const
	{
		return Tracer;
	}

	UAreaBoundaryComponent* GetArea() const
	{
		return ConversionDestination;
	}

private:
	UPROPERTY()
	UTracerPathComponent* Tracer;

	UPROPERTY()
	UAreaBoundaryComponent* ConversionDestination;

	bool bAreaExpansionAlreadyPending = false;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(Tracer);
		AddLifeDependency(ConversionDestination);

		ConversionDestination->GetBoundary().Observe(this, [this](auto&)
		{
			ConvertPathToArea(Tracer->GetRunningPath());
		});

		Tracer->GetLastCompletePath().ObserveIfValid(this, [this](const FSegmentArray2D& CompletePath)
		{
			ConvertPathToArea(CompletePath);
		});
	}

	void ConvertPathToArea(const FSegmentArray2D& Path)
	{
		if (bAreaExpansionAlreadyPending)
		{
			return;
		}

		bAreaExpansionAlreadyPending = true;

		// TODO 여기 Path = Path를 쓰지 않으면 const ref로 캡쳐돼서 MoveTemp에서 컴파일러 에러 발생함
		// 컴파일러 버그인지 coroutine lambda가 mutable일 때 capture의 특수 상황인지에 대해 조사 필요
		RunWeakCoroutine(this, [this, Path = Path]() mutable -> FWeakCoroutine
		{
			using FExpansionResult = UAreaBoundaryComponent::FExpansionResult;
			for (const FExpansionResult& Each : co_await ConversionDestination->ExpandByPath(MoveTemp(Path)))
			{
				OnTracerToAreaConversion.Broadcast(Each.CorrectlyAlignedPath);
			}
			bAreaExpansionAlreadyPending = false;
		});
	}
};
