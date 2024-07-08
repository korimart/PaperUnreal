// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaMeshComponent.h"
#include "VectorArray2DEventGenerator.h"
#include "Core/ActorComponent2.h"
#include "AreaMeshGeneratorComponent.generated.h"


UCLASS()
class UAreaMeshGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshSource(IVectorArray2DEventGenerator* Source)
	{
		MeshSource = Cast<UObject>(Source);
	}
	
	void SetMeshDestination(UAreaMeshComponent* Dest)
	{
		MeshDest = Dest;
	}

private:
	UPROPERTY()
	TScriptInterface<IVectorArray2DEventGenerator> MeshSource;
	
	UPROPERTY()
	UAreaMeshComponent* MeshDest;
	
	UAreaMeshGeneratorComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		check(AllValid(MeshSource, MeshDest));

		RunWeakCoroutine(this, [this](auto&) -> FWeakCoroutine
		{
			for (auto Events = MeshSource->CreateEventGenerator();;)
			{
				MeshDest->SetMeshByWorldBoundary((co_await Events.Next()).Affected);
			}
		});
	}
};
