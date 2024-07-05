// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "TracerToAreaConverterComponent.generated.h"


UCLASS()
class UTracerToAreaConverterComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTracerToAreaConversion, const FSegmentArray2D&, bool);
	FOnTracerToAreaConversion OnTracerToAreaConversion;

	void SetTracer(UTracerPathComponent* InTracer)
	{
		Tracer = InTracer;
	}
	
	void SetConversionDestination(UAreaMeshComponent* Destination)
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
	UAreaMeshComponent* ConversionDestination;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(Tracer, ConversionDestination));

		Tracer->OnPathEvent.AddWeakLambda(this, [this](const FTracerPathEvent& Event)
		{
			if (Event.GenerationEnded())
			{
				if (TOptional<UAreaMeshComponent::FExpansionResult> Result
					= ConversionDestination->ExpandByPath(Tracer->GetPath()))
				{
					OnTracerToAreaConversion.Broadcast(Tracer->GetPath(), Result->bAddedToTheLeftOfPath);
				}
			}
		});
	}
};
