// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"


template <typename T>
T* ValidOrNull(T* Object)
{
	return IsValid(Object) ? Object : nullptr;
}


template <typename... ElementTypes>
bool AreAllValid(const TWeakObjectPtr<ElementTypes>&... Check)
{
	return (Check.IsValid() && ...);
}


inline TWeakAwaitable<AGameStateBase*> WaitForGameState(UWorld* World)
{
	if (AGameStateBase* Ret = ValidOrNull(World->GetGameState()))
	{
		return Ret;
	}

	return WaitForBroadcast(World, World->GameStateSetEvent);
}


template <typename SoftObjectType>
TWeakAwaitable<SoftObjectType*> RequestAsyncLoad(UObject* Lifetime, const TSoftObjectPtr<SoftObjectType>& SoftPointer)
{
	TWeakAwaitable<SoftObjectType*> Ret;
	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPointer.ToSoftObjectPath(),
		Ret.CreateSetValueDelegate<FStreamableDelegate>(Lifetime, [SoftPointer]()
		{
			return SoftPointer.Get();
		}));
	return Ret;
}


template <typename SoftObjectType, typename CallbackType>
TWeakAwaitable<bool> RequestAsyncLoad(const TSoftObjectPtr<SoftObjectType>& SoftPointer, UObject* Lifetime, CallbackType&& Callback)
{
	TWeakAwaitable<bool> Ret;
	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPointer.ToSoftObjectPath(),
		Ret.CreateSetValueDelegate<FStreamableDelegate>(Lifetime, [SoftPointer, Callback = Forward<CallbackType>(Callback)]()
		{
			Callback(SoftPointer.Get());
			return true;
		}));
	return Ret;
}


#define DEFINE_REPPED_VAR_SETTER(VarName, NewValue)\
	check(GetNetMode() != NM_Client);\
	Repped##VarName = NewValue;\
	if (GetNetMode() != NM_DedicatedServer) OnRep_##VarName();
