// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerPathComponent.h"
#include "Core/ActorComponent2.h"
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

	bool bImConversionInstigator = false;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(Tracer, ConversionDestination));

		Tracer->GetTracerPathStreamer().GetOnStreamEnd().AddWeakLambda(this, [this]()
		{
			ConvertPathToArea();
		});

		ConversionDestination->OnBoundaryChanged.AddWeakLambda(this, [this](auto&)
		{
			ConvertPathToArea();
		});
	}

	void ConvertPathToArea()
	{
		using FExpansionResult = UAreaBoundaryComponent::FExpansionResult;

		if (bImConversionInstigator)
		{
			return;
		}

		bImConversionInstigator = true;
		for (const FExpansionResult& Each : ConversionDestination->ExpandByPath(Tracer->GetPath()))
		{
			OnTracerToAreaConversion.Broadcast(Each.CorrectlyAlignedPath);
		}
		bImConversionInstigator = false;
	}
};
