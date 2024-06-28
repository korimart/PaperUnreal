// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"
#include "GameFramework/GameStateBase.h"


template <typename T>
T* ValidOrNull(T* Object)
{
	return IsValid(Object) ? Object : nullptr;
}


inline TWeakAwaitable<AGameStateBase*> WaitForGameState(UWorld* World)
{
	if (AGameStateBase* Ret = ValidOrNull(World->GetGameState()))
	{
		return Ret;
	}

	return WaitForBroadcast(World, World->GameStateSetEvent);
}


#define DEFINE_REPPED_VAR_SETTER(VarName, NewValue)\
	check(GetNetMode() != NM_Client);\
	Repped##VarName = NewValue;\
	if (GetNetMode() != NM_DedicatedServer) OnRep_##VarName();