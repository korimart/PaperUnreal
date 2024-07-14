// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AreaBoundaryStream.h"
#include "AreaMeshComponent.h"
#include "Core/ActorComponent2.h"
#include "AreaMeshGeneratorComponent.generated.h"


UCLASS()
class UAreaMeshGeneratorComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	void SetMeshSource(IAreaBoundaryStream* Source)
	{
		MeshSource = Cast<UObject>(Source);
	}
	
	void SetMeshDestination(UAreaMeshComponent* Dest)
	{
		MeshDest = Dest;
	}

private:
	UPROPERTY()
	TScriptInterface<IAreaBoundaryStream> MeshSource;
	
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

		RunWeakCoroutine(this, [this](auto& Context) -> FWeakCoroutine
		{
			Context.AddToWeakList(MeshSource);
			Context.AddToWeakList(MeshDest);
			for (auto BoundaryStream = MeshSource->CreateBoundaryStream();;)
			{
				MeshDest->SetMeshByWorldBoundary(co_await BoundaryStream.Next());
			}
		});
	}
};
