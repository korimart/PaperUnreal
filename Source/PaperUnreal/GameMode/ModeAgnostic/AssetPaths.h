// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


struct FAssetPaths
{
	inline static FSoftObjectPath ExplosionActorPath{TEXT("/Script/Engine.Blueprint'/Game/ExplostionFromStarterContent/Blueprint_Effect_Explosion.Blueprint_Effect_Explosion_C'")};
	static TSoftClassPtr<AActor> SoftExplosionActor() { return TSoftClassPtr<AActor>{ExplosionActorPath}; }

	inline static FSoftObjectPath BaseColorMaterial{TEXT("/Script/Engine.Material'/Game/LevelPrototyping/Materials/M_Solid.M_Solid'")};
	static TSoftObjectPtr<UMaterial> SoftBaseColorMaterial() { return TSoftObjectPtr<UMaterial>{BaseColorMaterial}; }
};
