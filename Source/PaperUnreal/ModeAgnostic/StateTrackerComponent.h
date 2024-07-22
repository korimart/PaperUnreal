// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperUnreal/GameFramework2/ActorComponent2.h"
#include "PaperUnreal/GameFramework2/Utils.h"
#include "PaperUnreal/WeakCoroutine/CancellableFuture.h"
#include "StateTrackerComponent.generated.h"


USTRUCT()
struct FNamedCallbackId
{
	GENERATED_BODY()
	
	UPROPERTY()
	UObject* Owner;

	UPROPERTY()
	FName CallbackName;
	
	friend uint32 GetTypeHash(const FNamedCallbackId& OfThis)
	{
		// 적절히 좋은 해쉬 생성에 대한 ChatGPT4o의 추천
		return GetTypeHash(OfThis.Owner) ^ (GetTypeHash(OfThis.CallbackName) << 1);
	}

	friend bool operator==(const FNamedCallbackId& Left, const FNamedCallbackId& Right)
	{
		return Left.Owner == Right.Owner && Left.CallbackName == Right.CallbackName;
	}
};


UCLASS()
class UStateTrackerComponent : public UActorComponent2
{
	GENERATED_BODY()

protected:
	UStateTrackerComponent()
	{
		bWantsInitializeComponent = true;
	}

	virtual void UninitializeComponent() override
	{
		Super::UninitializeComponent();

		CallbackHandles.Empty();
		Promises.Empty();
	}

	FDelegateSPHandle& UniqueHandle(UObject* Owner, FName Name)
	{
		return CallbackHandles.FindOrAdd({Owner, Name});
	}
	
	void RemoveHandles(UObject* Owner)
	{
		TArray<FNamedCallbackId> Keys;
		CallbackHandles.GetKeys(Keys);
		for (const FNamedCallbackId& Each : Keys)
		{
			if (Each.Owner == Owner)
			{
				CallbackHandles.Remove(Each);
			}
		}
	}

	TCancellableFuture<void> MakeConditionedPromise(const TFunction<bool()>& Condition)
	{
		if (Condition())
		{
			return {};
		}
		
		auto [Promise, Future] = MakePromise<void>();
		Promises.Add({.Promise = MoveTemp(Promise), .Condition = Condition,});
		return MoveTemp(Future);
	}

	void OnSomeConditionMaybeSatisfied()
	{
		TArray<TCancellablePromise<void>> PromisesToFulfill;
		
		for (int32 i = Promises.Num() - 1; i >= 0; --i)
		{
			if (Promises[i].Condition())
			{
				PromisesToFulfill.Add(MoveTemp(Promises[i].Promise));
				Promises.RemoveAt(i);
			}
		}

		for (auto& Each : PromisesToFulfill)
		{
			Each.SetValue();
		}
	}

private:
	UPROPERTY()
	TMap<FNamedCallbackId, FDelegateSPHandle> CallbackHandles;

	struct FConditionedPromise
	{
		TCancellablePromise<void> Promise;
		TFunction<bool()> Condition;
	};

	TArray<FConditionedPromise> Promises;
};
