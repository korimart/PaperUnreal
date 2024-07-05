// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TracerMeshComponent.h"
#include "TracerPathGenerator.h"
#include "Core/ActorComponentEx.h"
#include "ReplicatedTracerPathComponent.generated.h"


UCLASS()
class UReplicatedTracerPathComponent : public UActorComponentEx, public ITracerPathGenerator
{
	GENERATED_BODY()

public:
	virtual TValueGenerator<FTracerPathEvent> CreatePathEventGenerator() override
	{
		return CreateMulticastValueGenerator(this, TArray<FTracerPathEvent>{}, OnPathEvent);
	}
	
	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		ServerTracerPath = Source;
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	FSegmentArray2D Path;

	UReplicatedTracerPathComponent()
	{
		SetIsReplicatedByDefault(true);
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	}
};
