// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerToAreaConverterComponent.h"
#include "AreaSlasherComponent.generated.h"


UCLASS()
class UAreaSlasherComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetSlashTarget(UAreaBoundaryComponent* Target)
	{
		check(!HasBeenInitialized())
		SlashTarget = Target;
	}

	void SetTracerToAreaConverter(UTracerToAreaConverterComponent* Converter)
	{
		check(!HasBeenInitialized())
		TracerToAreaConverter = Converter;
	}

private:
	UPROPERTY()
	UAreaBoundaryComponent* SlashTarget;

	UPROPERTY()
	UTracerToAreaConverterComponent* TracerToAreaConverter;

	UAreaSlasherComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(SlashTarget);
		AddLifeDependency(TracerToAreaConverter);

		TracerToAreaConverter->OnTracerToAreaConversion.AddWeakLambda(
			this, [this](FSegmentArray2D CorrectlyAlignedPath)
			{
				if (TracerToAreaConverter->GetArea()->IsInside(SlashTarget))
				{
					SlashTarget->Reset();
				}
				else
				{
					CorrectlyAlignedPath.ReverseVertexOrder();
					SlashTarget->ReduceByPath(CorrectlyAlignedPath);
				}
			});
	}
};
