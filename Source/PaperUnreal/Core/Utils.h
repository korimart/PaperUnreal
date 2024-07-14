// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UECoroutine.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"


namespace Utils_Private
{
	inline bool IsValid(const UObject* Pointer)
	{
		return ::IsValid(Pointer);
	}

	template <typename T>
	bool IsValid(const TScriptInterface<T>& Pointer)
	{
		return ::IsValid(Pointer.GetObject());
	}

	template <typename SmartPointerType>
	concept CSmartPointer = requires(SmartPointerType SmartPointer)
	{
		SmartPointer.IsValid();
	};

	bool IsValid(CSmartPointer auto SmartPointer)
	{
		return SmartPointer.IsValid();
	}

	bool IsValid(const auto& AnyObjectThatIsNotPointer)
	{
		return true;
	}


	template <typename FuncType>
	class TFinally
	{
	public:
		TFinally(FuncType InFunc) : Func(InFunc)
		{
		}

		TFinally(const TFinally&) = delete;
		TFinally& operator=(const TFinally&) = delete;
		TFinally& operator=(TFinally&&) = delete;

		TFinally(TFinally&& Other)
		{
			Func = Other.Func;
			Other.Func.Reset();
		}

		~TFinally()
		{
			if (Func)
			{
				(*Func)();
			}
		}

	private:
		TOptional<FuncType> Func;
	};
}


template <typename T>
T* ValidOrNull(T* Object)
{
	return IsValid(Object) ? Object : nullptr;
}


template <typename T>
T* ValidOrNull(T* const* Object)
{
	return Object ? *Object : nullptr;
}


bool AllValid(const auto&... Check)
{
	return (Utils_Private::IsValid(Check) && ...);
}


template <typename T>
TWeakObjectPtr<T> ToWeak(T* Object)
{
	return {Object};
}


template <typename T>
bool IsNearlyLE(T Left, T Right)
{
	return FMath::IsNearlyEqual(Left, Right) || Left < Right;
}


template <typename FuncType>
UE_NODISCARD auto Finally(FuncType&& Func)
{
	return Utils_Private::TFinally<FuncType>{Forward<FuncType>(Func)};
}


template <typename T>
void FindAndDestroyComponent(AActor* Actor)
{
	if (auto Found = Actor->FindComponentByClass<T>())
	{
		Found->DestroyComponent();
	}
}


inline TWeakAwaitable<bool> WaitOneTick(UWorld* World)
{
	TWeakAwaitable<bool> Ret;
	World->GetTimerManager().SetTimerForNextTick(
		Ret.CreateSetValueDelegate<FTimerDelegate>([]()
		{
			return true;
		}));
	return Ret;
}


inline TWeakAwaitable<AGameStateBase*> WaitForGameState(UWorld* World)
{
	if (AGameStateBase* Ret = ValidOrNull(World->GetGameState()))
	{
		return Ret;
	}

	return WaitForBroadcast(World->GameStateSetEvent);
}


// TODO 바로 수령하지 않으면 garbage를 반환할 수 있음
template <typename SoftObjectType>
TWeakAwaitable<SoftObjectType*> RequestAsyncLoad(const TSoftObjectPtr<SoftObjectType>& SoftPointer)
{
	TWeakAwaitable<SoftObjectType*> Ret;
	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPointer.ToSoftObjectPath(),
		Ret.CreateSetValueDelegate<FStreamableDelegate>([SoftPointer]()
		{
			return SoftPointer.Get();
		}));
	return Ret;
}


#define DEFINE_REPPED_VAR_SETTER(VarName, NewValue)\
	check(GetNetMode() != NM_Client);\
	Rep##VarName = NewValue;\
	OnRep_##VarName();


#define RETURN_IF_FALSE(boolean) if (!boolean) return true;
