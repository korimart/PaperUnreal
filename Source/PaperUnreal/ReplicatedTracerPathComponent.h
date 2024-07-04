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
	virtual FSimpleMulticastDelegate& GetOnNewPointGenerated() override { return OnNewPointGenerated; };
	virtual FSimpleMulticastDelegate& GetOnLastPointModified() override { return OnLastPointModified; }
	virtual FSimpleMulticastDelegate& GetOnCleared() override { return OnCleared; }
	virtual FSimpleMulticastDelegate& GetOnGenerationStarted() override { return OnGenerationStarted; }
	virtual FSimpleMulticastDelegate& GetOnGenerationEnded() override { return OnGenerationEnded; }
	virtual const FSegmentArray2D& GetPath() const override { return Path; }

	void SetTracerPathSource(UTracerPathComponent* Source)
	{
		check(GetNetMode() != NM_Client);
		ServerTracerPath = Source;
	}

private:
	UPROPERTY()
	UTracerPathComponent* ServerTracerPath;

	FSegmentArray2D Path;

	FSimpleMulticastDelegate OnNewPointGenerated;
	FSimpleMulticastDelegate OnLastPointModified;
	FSimpleMulticastDelegate OnCleared;
	FSimpleMulticastDelegate OnGenerationStarted;
	FSimpleMulticastDelegate OnGenerationEnded;

	UReplicatedTracerPathComponent()
	{
		SetIsReplicatedByDefault(true);
	}
};
