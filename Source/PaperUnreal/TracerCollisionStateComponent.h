// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "TracerMeshComponent.h"
#include "Core/ActorComponentEx.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "TracerCollisionStateComponent.generated.h"


UCLASS()
class UTracerCollisionStateComponent : public UActorComponentEx
{
	GENERATED_BODY()

public:
	void SetTracer(UTracerPathComponent* InTracer)
	{
		Tracer = InTracer;
	}

	TLiveDataView<bool> FindOrAddCollisionWith(UAreaMeshComponent* AreaMeshComponent)
	{
		auto& Ret = AreaToCollisionStatusMap.FindOrAdd(AreaMeshComponent);
		NotifyAreaCollisionWithTracer();
		return Ret;
	}

private:
	UPROPERTY()
	UTracerPathComponent* Tracer;
	
	TMap<TWeakObjectPtr<UAreaMeshComponent>, TLiveData<bool>> AreaToCollisionStatusMap;

	UTracerCollisionStateComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();
		
		check(AllValid(Tracer));

		Tracer->OnNewPointGenerated.AddUObject(this, &ThisClass::NotifyAreaCollisionWithTracer);
		Tracer->OnLastPointModified.AddUObject(this, &ThisClass::NotifyAreaCollisionWithTracer);
	}

	void NotifyAreaCollisionWithTracer()
	{
		const FVector2D TracerFront = Tracer->GetPath().GetLastPoint();

		for (auto& [EachArea, EachStatus] : AreaToCollisionStatusMap)
		{
			// TODO remove if not valid
			if (EachArea.IsValid())
			{
				// TODO remove if not bound (except when FindOrAdd was called)
				EachStatus.SetValue(EachArea->IsInside(FVector{TracerFront, 0.f}));
			}
		}
	}
};
