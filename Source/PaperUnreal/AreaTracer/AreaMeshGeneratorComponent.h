// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryProvider.h"
#include "AreaMeshComponent.h"
#include "PaperUnreal/WeakCoroutine/WeakCoroutine.h"
#include "AreaMeshGeneratorComponent.generated.h"


UCLASS()
class UAreaMeshGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshSource(IAreaBoundaryProvider* Source)
	{
		MeshSource = Cast<UObject>(Source);
	}
	
	void SetMeshDestination(UAreaMeshComponent* Dest)
	{
		MeshDest = Dest;
	}

private:
	UPROPERTY()
	TScriptInterface<IAreaBoundaryProvider> MeshSource;
	
	UPROPERTY()
	UAreaMeshComponent* MeshDest;
	
	UAreaMeshGeneratorComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();

		AddLifeDependency(Cast<UActorComponent2>(MeshSource.GetObject()));
		AddLifeDependency(MeshDest);

		RunWeakCoroutine(this, [this](FWeakCoroutineContext& Context) -> FWeakCoroutine
		{
			for (auto BoundaryStream = MeshSource->GetBoundary().CreateStream();;)
			{
				MeshDest->SetMeshByWorldBoundary(co_await BoundaryStream);
			}
		});
	}
};
