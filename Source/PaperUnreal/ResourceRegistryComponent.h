// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponent2.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "ResourceRegistryComponent.generated.h"


UCLASS(Within=GameStateBase)
class UResourceRegistryComponent : public UActorComponent2
{
	GENERATED_BODY()

public:
	DECLARE_LIVE_DATA_AND_GETTER(bool, bResourcesLoaded);

	UMaterialInstance* GetAreaMaterialFor(int32 TeamIndex) const
	{
		return TeamToAreaMaterialMap.FindRef(TeamIndex);
	}

	UMaterialInstance* GetTracerMaterialFor(int32 TeamIndex) const
	{
		return TeamToTracerMaterialMap.FindRef(TeamIndex);
	}

private:
	UPROPERTY()
	TMap<int32, UMaterialInstance*> TeamToAreaMaterialMap;

	UPROPERTY()
	TMap<int32, UMaterialInstance*> TeamToTracerMaterialMap;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (GetNetMode() == NM_DedicatedServer)
		{
			return;
		}

		RunWeakCoroutine(this, [this](FWeakCoroutineContext&) -> FWeakCoroutine
		{
			// TODO 데디에서 머티리얼의 종류를 레플리케이트 받도록 수정할 수 있음
			const TSoftObjectPtr<UMaterialInstance> SoftSolidBlue
			{
				FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue.MI_Solid_Blue'")}
			};

			const TSoftObjectPtr<UMaterialInstance> SoftSolidBlueLight
			{
				FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Blue_Light.MI_Solid_Blue_Light'")}
			};

			const TSoftObjectPtr<UMaterialInstance> SoftSolidRed
			{
				FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red.MI_Solid_Red'")}
			};

			const TSoftObjectPtr<UMaterialInstance> SoftSolidRedLight
			{
				FSoftObjectPath{TEXT("/Script/Engine.MaterialInstanceConstant'/Game/LevelPrototyping/Materials/MI_Solid_Red_Light.MI_Solid_Red_Light'")}
			};

			auto SolidBlueAwaitable = RequestAsyncLoad(SoftSolidBlue, this, [this](UMaterialInstance* Material)
			{
				TeamToAreaMaterialMap.FindOrAdd(0) = Material;
			});
			
			auto SolidRedAwaitable = RequestAsyncLoad(SoftSolidRed, this, [this](UMaterialInstance* Material)
			{
				TeamToAreaMaterialMap.FindOrAdd(1) = Material;
			});
			
			auto SolidBlueLightAwaitable = RequestAsyncLoad(SoftSolidBlueLight, this, [this](UMaterialInstance* Material)
			{
				TeamToTracerMaterialMap.FindOrAdd(0) = Material;
			});
			
			auto SolidRedLightAwaitable = RequestAsyncLoad(SoftSolidRedLight, this, [this](UMaterialInstance* Material)
			{
				TeamToTracerMaterialMap.FindOrAdd(1) = Material;
			});
			
			co_await SolidBlueAwaitable;
			co_await SolidRedAwaitable;
			co_await SolidBlueLightAwaitable;
			co_await SolidRedLightAwaitable;
			bResourcesLoaded.SetValue(true);
		});
	}
};
