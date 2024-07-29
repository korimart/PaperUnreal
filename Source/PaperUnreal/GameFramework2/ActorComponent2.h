// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent2.generated.h"


UCLASS()
class UActorComponent2 : public UActorComponent
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnBeginPlay;
	FSimpleMulticastDelegate OnEndPlay;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void AddLifeDependency(UActorComponent2* Dependency);

	template <typename T>
	T* NewChildComponent(AActor* Actor)
	{
		T* Component = NewObject<T>(Actor);
		Component->AddLifeDependency(this);
		return Component;
	}
};