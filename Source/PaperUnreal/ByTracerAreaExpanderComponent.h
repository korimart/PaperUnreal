// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracingMeshComponent.h"
#include "Components/ActorComponent.h"
#include "ByTracerAreaExpanderComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UByTracerAreaExpanderComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		TracingMeshComponent = GetOwner()->FindComponentByClass<UTracingMeshComponent>();
		check(IsValid(TracingMeshComponent));
		
		GEngine->GameViewport->OnInputKey().AddWeakLambda(this, [this](const FInputKeyEventArgs& EventArgs)
		{
			if (EventArgs.Key == EKeys::One && EventArgs.Event == EInputEvent::IE_Pressed)
			{
				TracingMeshComponent->SetDrawingEnabled(true);
			}
			
			if (EventArgs.Key == EKeys::Two && EventArgs.Event == EInputEvent::IE_Pressed)
			{
				TracingMeshComponent->SetDrawingEnabled(false);
			}
		});
	}

	UPROPERTY()
	UTracingMeshComponent* TracingMeshComponent;
};