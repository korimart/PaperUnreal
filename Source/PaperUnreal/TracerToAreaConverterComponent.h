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

	bool bImConversionInstigator = false;

	UTracerToAreaConverterComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(Tracer);
		AddLifeDependency(ConversionDestination);

		Tracer->GetTracerPathStreamer().OnStreamEnd(this, [this]()
		{
			ConvertPathToArea();
		});

		ConversionDestination->GetBoundaryStreamer().Observe(this, [this](auto&)
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
		for (const FExpansionResult& Each : ConversionDestination->ExpandByPath(Tracer->GetTracerPath()))
		{
			OnTracerToAreaConversion.Broadcast(Each.CorrectlyAlignedPath);
		}
		bImConversionInstigator = false;
	}
};
