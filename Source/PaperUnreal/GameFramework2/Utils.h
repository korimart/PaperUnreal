// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/WeakCoroutine/TypeTraits.h"
#include "Utils.generated.h"


namespace Utils_Private
{
	template <typename FuncType>
	class TFinally
	{
	public:
		TFinally(FuncType InFunc) : Func(InFunc)
		{
		}

		TFinally(const TFinally&) = delete;
		TFinally& operator=(const TFinally&) = delete;

		TFinally(TFinally&& Other) noexcept
		{
			Func = Other.Func;
			Other.Func.Reset();
		}

		TFinally& operator=(TFinally&& Other) noexcept
		{
			Func = Other.Func;
			Other.Func.Reset();
			return *this;
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


USTRUCT()
struct FDelegateSPHandle
{
	GENERATED_BODY()

	const TSharedRef<bool>& ToShared() const { return Life; }

private:
	TSharedRef<bool> Life = MakeShared<bool>();
};


class FTickingSwitch
{
public:
	void Tick(bool bNewBool)
	{
		bPrevTick = bThisTick;
		bThisTick = bNewBool;
	}

	void IfSwitchedOnThisFrame(const auto& Func)
	{
		if (!bPrevTick && bThisTick)
		{
			Func();
		}
	}

	void IfTrueThisFrame(const auto& Func)
	{
		if (bThisTick)
		{
			Func();
		}
	}

	void IfSwitchedOffThisFrame(const auto& Func)
	{
		if (bPrevTick && !bThisTick)
		{
			Func();
		}
	}

private:
	bool bPrevTick = false;
	bool bThisTick = false;
};


template <typename T>
class TTickingValue
{
public:
	void Tick(TOptional<T> Value)
	{
		PrevTick = MoveTemp(ThisTick);
		ThisTick = MoveTemp(Value);
	}

	void OverTwoTicks(const auto& Func)
	{
		if (PrevTick && ThisTick)
		{
			Func(*PrevTick, *ThisTick);
		}
	}

private:
	TOptional<T> PrevTick;
	TOptional<T> ThisTick;
};


template <typename T>
T* ValidOrNull(T* Object)
{
	return IsValid(Object) ? Object : nullptr;
}


template <typename T>
T* ValidOrNull(T** Object)
{
	return Object ? *Object : nullptr;
}


template <typename... ArgTypes>
bool AllValid(const ArgTypes&... Check)
{
	return (::IsValid(TUObjectUnsafeWrapperTypeTraits<ArgTypes>::GetUObject(Check)) && ...);
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


template <typename FuncType>
UE_NODISCARD auto FinallyIfValid(UObject* Object, FuncType&& Func)
{
	return Finally([Object = TWeakObjectPtr{Object}, Func = Forward<FuncType>(Func)]()
	{
		if (Object.IsValid())
		{
			Func();
		}
	});
}


template <typename T>
T PopFront(TArray<T>& Array, int32 Index = 0)
{
	T Ret = MoveTemp(Array[Index]);
	Array.RemoveAt(Index);
	return Ret;
}


template <typename T, typename U>
TArray<T*> GetComponents(const TArray<U>& Actors)
{
	TArray<T*> Ret;
	for (auto Each : Actors)
	{
		if (T* Found = Each->template FindComponentByClass<T>())
		{
			Ret.Add(Found);
		}
	}
	return Ret;
}


template <typename... ComponentTypes, typename ActorType, typename FuncType>
void ForEachComponent(const TArray<ActorType*>& Actors, const FuncType& Func)
{
	const auto FindComponentsAndInvokeFunc
		= [&]<typename ToFind, typename... Rest>(auto& Self, TTypeList<ToFind, Rest...>, ActorType* Actor, auto&... Found)
	{
		if (auto Component = Actor->template FindComponentByClass<ToFind>())
		{
			if constexpr (sizeof...(Rest) == 0)
			{
				Func(Found..., Component);
			}
			else
			{
				Self(Self, TTypeList<Rest...>{}, Actor, Found..., Component);
			}
		}
	};

	for (ActorType* Each : Actors)
	{
		if (IsValid(Each))
		{
			FindComponentsAndInvokeFunc(FindComponentsAndInvokeFunc, TTypeList<ComponentTypes...>{}, Each);
		}
	}
}


template <typename T>
void FindAndDestroyComponent(AActor* Actor)
{
	if (auto Found = Actor->FindComponentByClass<T>())
	{
		Found->DestroyComponent();
	}
}


inline FLinearColor NonEyeSoaringRandomColor()
{
	return FLinearColor::MakeFromHSV8(FMath::RandRange(0, 255), 220, 100);
}


inline FLinearColor ALittleBrighter(const FLinearColor& Color)
{
	FLinearColor HSV = Color.LinearRGBToHSV();
	HSV.B *= 1.75f;
	return HSV.HSVToLinearRGB();
}


#define RETURN_IF_FALSE(boolean) if (!boolean) return true;
