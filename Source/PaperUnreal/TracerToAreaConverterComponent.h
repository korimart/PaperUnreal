// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerPathComponent.h"
#include "Core/ActorComponent2.h"
#include "TracerToAreaConverterComponent.generated.h"


UCLASS()
class UTracerToAreaConverterComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTracerToAreaConversion, const FSegmentArray2D&, bool);
	FOnTracerToAreaConversion OnTracerToAreaConversion;

	void SetTracer(UTracerPathComponent* InTracer)
	{
		Tracer = InTracer;
	}
	
	void SetConversionDestination(UAreaBoundaryComponent* Destination)
	{
		ConversionDestination = Destination;
	}

	UTracerPathComponent* GetTracer() const
	{
		return Tracer;
	}

private:
	UPROPERTY()
	UTracerPathComponent* Tracer;

	UPROPERTY()
	UAreaBoundaryComponent* ConversionDestination;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(Tracer, ConversionDestination));

		Tracer->OnCompletePathGenerated.AddWeakLambda(this, [this](const FSegmentArray2D& CompletePath)
		{
			if (TOptional<UAreaBoundaryComponent::FExpansionResult> Result
				= ConversionDestination->ExpandByPath(CompletePath))
			{
				OnTracerToAreaConversion.Broadcast(CompletePath, Result->bAddedToTheLeftOfPath);
			}
		});
	}
};
