// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


struct FAssetPaths
{
	inline static FSoftObjectPath ExplosionActorPath{TEXT("/Game/ExplostionFromStarterContent/Blueprint_Effect_Explosion")};
	static TSoftClassPtr<AActor> SoftExplosionActor() { return TSoftClassPtr<AActor>{ExplosionActorPath}; }

	inline static FSoftObjectPath BaseColorMaterial{TEXT("/Game/LevelPrototyping/Materials/M_Solid")};
	static TSoftObjectPtr<UMaterial> SoftBaseColorMaterial() { return TSoftObjectPtr<UMaterial>{BaseColorMaterial}; }
};
