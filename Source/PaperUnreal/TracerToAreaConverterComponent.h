// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OffAreaTracerGenEnablerComponent.h"
#include "Core/ActorComponentEx.h"
#include "TracerToAreaConverterComponent.generated.h"


UCLASS()
class UTracerToAreaConverterComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTracerToAreaConversion, const FSegmentArray2D&, bool);
	FOnTracerToAreaConversion OnTracerToAreaConversion;

	void SetTracerGenController(UOffAreaTracerGenEnablerComponent* Controller)
	{
		TracerGenController = Controller;
	}

	void SetConversionDestination(UAreaMeshComponent* Destination)
	{
		ConversionDestination = Destination;
	}

private:
	UPROPERTY()
	UOffAreaTracerGenEnablerComponent* TracerGenController;

	UPROPERTY()
	UTracerMeshComponent* Tracer;

	UPROPERTY()
	UAreaMeshComponent* ConversionDestination;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(TracerGenController, ConversionDestination));

		Tracer = TracerGenController->GetControlledGenerator()->GetGenDestination();
		check(IsValid(Tracer));

		TracerGenController->OnGenPreEnable.AddWeakLambda(this, [this]()
		{
			Tracer->Reset();
		});

		TracerGenController->OnGenPostDisable.AddWeakLambda(this, [this]()
		{
			if (TOptional<UAreaMeshComponent::FExpansionResult> Result
				= ConversionDestination->ExpandByPath(Tracer->GetCenterSegmentArray2D()))
			{
				OnTracerToAreaConversion.Broadcast(Tracer->GetCenterSegmentArray2D(), Result->bAddedToTheLeftOfPath);
			}
		});
	}
};
