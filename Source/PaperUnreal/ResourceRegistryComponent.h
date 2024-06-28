// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActorComponentEx.h"
#include "Core/LiveData.h"
#include "Core/Utils.h"
#include "ResourceRegistryComponent.generated.h"


UCLASS(Within=GameStateBase)
class UResourceRegistryComponent : public UActorComponentEx
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

		RunWeakCoroutine(this, [this]() -> FWeakCoroutine
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
			
			auto SolidBlueAwaitable = RequestAsyncLoad(this, SoftSolidBlue);
			auto SolidBlueLightAwaitable = RequestAsyncLoad(this, SoftSolidBlueLight);
			auto SolidRedAwaitable = RequestAsyncLoad(this, SoftSolidRed);
			auto SolidRedLightAwaitable = RequestAsyncLoad(this, SoftSolidRedLight);

			// TODO 여기서 리소스들이 살아있다는 보장이 없음 해결해야 됨

			TeamToAreaMaterialMap.FindOrAdd(0) = co_await SolidBlueAwaitable;
			TeamToAreaMaterialMap.FindOrAdd(1) = co_await SolidRedAwaitable;
			TeamToTracerMaterialMap.FindOrAdd(0) = co_await SolidBlueLightAwaitable;
			TeamToTracerMaterialMap.FindOrAdd(1) = co_await SolidRedLightAwaitable;

			bResourcesLoaded.SetValue(true);
		});
	}
};
